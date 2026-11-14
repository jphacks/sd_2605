require "test_helper"
require_relative "../support/push_test_helpers"

class WebPushNoticeTest < ActiveSupport::TestCase
  include PushTestHelpers
  include ActiveJob::TestHelper
  setup do
    configure_push
    @resident = User.create!(email: "notice-resident-#{SecureRandom.hex(4)}@example.com", password: "password1")
    @messenger = User.create!(email: "notice-messenger-#{SecureRandom.hex(4)}@example.com", password: "password1")
    @outsider = User.create!(email: "notice-other-#{SecureRandom.hex(4)}@example.com", password: "password1")
    @device = Device.create!(id: "notice-#{SecureRandom.hex(4)}", user: @resident, password: "device-pass-1")
    @device.grant_access_to!(@messenger, role: "messenger")
    @subscription = browser_subscription(@messenger)
    browser_subscription(@resident)
    browser_subscription(@outsider)
    @event = @device.home_events.create!(id: "event-#{SecureRandom.hex(8)}", event_type: "arrived_home", occurred_at: Time.current)
    clear_enqueued_jobs
    ActionMailer::Base.deliveries.clear
  end
  teardown { restore_push }

  test "only messengers receive a durable outbox entry and mail stays paused" do
    assert_enqueued_jobs 1, only: PushDeliveryJob do
      assert HomeEventNotice.deliver!(@event)
    end
    assert_equal [ @subscription.id ], @event.push_deliveries.pluck(:push_subscription_id)
    HomeEventNotice.deliver!(@event)
    assert_equal 1, @event.push_deliveries.count
    assert_empty ActionMailer::Base.deliveries
  end

  test "successful jobs are not sent twice and contain an authenticated history destination" do
    WebPushNotice.enqueue!(@event)
    delivery = @event.push_deliveries.first
    sent = []
    with_method(WebPushTransport, :deliver, ->(subscription, payload) { sent << [ subscription.id, payload ]; :sent }) do
      2.times { PushDeliveryJob.perform_now(delivery.id) }
    end
    assert_equal 1, sent.size
    assert delivery.reload.sent_at
    assert_equal "帰宅しました", sent.first.last[:title]
    uri = URI.parse(sent.first.last[:url])
    assert_equal "/history", uri.path
    assert_equal @device.id, URI.decode_www_form(uri.query).to_h["device_id"]
    assert_equal @event.id, URI.decode_www_form(uri.query).to_h["event_id"]
  end

  test "failed jobs retain retry state and heartbeat recovers due rows after a restart" do
    WebPushNotice.enqueue!(@event)
    delivery = @event.push_deliveries.first
    with_method(WebPushTransport, :deliver, ->(*) { :failed }) { PushDeliveryJob.perform_now(delivery.id) }
    assert_nil delivery.reload.sent_at
    assert_equal 1, delivery.attempts
    assert delivery.next_attempt_at > Time.current
    clear_enqueued_jobs
    assert_no_enqueued_jobs { WebPushNotice.recover_pending!(@device) }
    travel_to(delivery.next_attempt_at + 1.second) do
      assert_enqueued_jobs 1, only: PushDeliveryJob do
        WebPushNotice.recover_pending!(@device)
      end
      with_method(WebPushTransport, :deliver, ->(*) { :sent }) { PushDeliveryJob.perform_now(delivery.id) }
    end
    assert delivery.reload.sent_at
  end

  test "revoked messenger access cancels a queued delivery" do
    WebPushNotice.enqueue!(@event)
    delivery = @event.push_deliveries.first
    @device.device_memberships.find_by!(user: @messenger).destroy!
    with_method(WebPushTransport, :deliver, ->(*) { flunk "revoked account must not receive a push" }) do
      PushDeliveryJob.perform_now(delivery.id)
    end
    assert delivery.reload.canceled_at
    assert_nil delivery.sent_at
  end

  test "expired subscriptions are removed without breaking other delivery records" do
    WebPushNotice.enqueue!(@event)
    response = Net::HTTPGone.new("1.1", "410", "Gone")
    response.instance_variable_set(:@read, true)
    response.instance_variable_set(:@body, "expired")
    with_method(WebPush, :payload_send, ->(**) { raise WebPush::ExpiredSubscription.new(response, "web.push.apple.com") }) do
      PushDeliveryJob.perform_now(@event.push_deliveries.first.id)
    end
    assert_not PushSubscription.exists?(@subscription.id)
    assert_empty @event.push_deliveries
  end

  test "provider requests use bounded waits and never contain WAV data" do
    captured = nil
    with_method(WebPush, :payload_send, ->(**options) { captured = options }) do
      assert_equal :sent, WebPushTransport.deliver(@subscription, WebPushNotice.payload(@event))
    end
    assert_equal 3, captured[:open_timeout]
    assert_equal 5, captured[:read_timeout]
    assert_equal 3600, captured[:ttl]
    assert_not_includes captured[:message], "RIFF"
    assert_not_includes captured[:message], ENV["WEB_PUSH_PRIVATE_KEY"]
  end

  test "a delivery stops retrying after the bounded attempt limit" do
    WebPushNotice.enqueue!(@event)
    delivery = @event.push_deliveries.first
    delivery.update!(attempts: PushDelivery::MAX_ATTEMPTS)
    clear_enqueued_jobs
    with_method(WebPushTransport, :deliver, ->(*) { flunk "exhausted delivery must not send" }) do
      PushDeliveryJob.perform_now(delivery.id)
    end
    assert_no_enqueued_jobs { WebPushNotice.recover_pending!(@device) }
    assert_nil delivery.reload.sent_at
  end
end
