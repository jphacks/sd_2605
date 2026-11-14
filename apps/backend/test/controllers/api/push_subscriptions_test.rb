require "test_helper"
require_relative "../../support/push_test_helpers"

module Api
  class PushSubscriptionsTest < ActionDispatch::IntegrationTest
    include PushTestHelpers
    setup do
      configure_push
      @resident = User.create!(email: "push-resident-#{SecureRandom.hex(4)}@example.com", password: "password1")
      @messenger = User.create!(email: "push-messenger-#{SecureRandom.hex(4)}@example.com", password: "password1")
      @other = User.create!(email: "push-other-#{SecureRandom.hex(4)}@example.com", password: "password1")
      @device = Device.create!(id: "push-#{SecureRandom.hex(4)}", user: @resident, password: "device-pass-1")
      @device.grant_access_to!(@messenger, role: "messenger")
      @device.grant_access_to!(@other, role: "messenger")
      @headers = { "Authorization" => "Bearer #{JsonWebToken.encode(@messenger.id).first}" }
      @valid = { endpoint: "https://web.push.apple.com/browser-#{SecureRandom.hex(8)}",
        keys: { p256dh: WebPush.generate_key.public_key, auth: Base64.urlsafe_encode64(SecureRandom.random_bytes(16), padding: false) } }
    end
    teardown { restore_push }

    test "requires authentication and exposes only the public VAPID key" do
      get "/api/push_config"
      assert_response :unauthorized
      get "/api/push_config", headers: @headers
      assert_response :success
      assert_equal true, response.parsed_body["configured"]
      assert_equal true, response.parsed_body["eligible"]
      assert_equal ENV["WEB_PUSH_PUBLIC_KEY"], response.parsed_body["public_key"]
      assert_not_includes response.body, ENV["WEB_PUSH_PRIVATE_KEY"]
    end

    test "creates checks updates and deletes a browser subscription without duplicates" do
      2.times do
        post "/api/push_subscriptions", params: { subscription: @valid }, headers: @headers, as: :json
        assert_response :created
      end
      assert_equal 1, @messenger.push_subscriptions.count
      post "/api/push_subscriptions/check", params: { endpoint: @valid[:endpoint] }, headers: @headers, as: :json
      assert_equal true, response.parsed_body["subscribed"]
      delete "/api/push_subscriptions", params: { endpoint: @valid[:endpoint] }, headers: @headers, as: :json
      assert_response :no_content
      assert_empty @messenger.push_subscriptions
    end

    test "resident cannot opt in and one account cannot inspect or delete another account subscription" do
      post "/api/push_subscriptions", params: { subscription: @valid }, headers: { "Authorization" => "Bearer #{JsonWebToken.encode(@resident.id).first}" }, as: :json
      assert_response :forbidden
      subscription = browser_subscription(@other)
      post "/api/push_subscriptions/check", params: { endpoint: subscription.endpoint }, headers: @headers, as: :json
      assert_equal false, response.parsed_body["subscribed"]
      delete "/api/push_subscriptions", params: { endpoint: subscription.endpoint }, headers: @headers, as: :json
      assert PushSubscription.exists?(subscription.id)
    end

    test "rejects internal arbitrary insecure endpoints and malformed encryption keys" do
      [ "http://web.push.apple.com/test", "https://127.0.0.1/test", "https://example.com/test",
        "https://web.push.apple.com.attacker.invalid/test", "https://user@web.push.apple.com/test", "https://web.push.apple.com:444/test" ].each do |endpoint|
        post "/api/push_subscriptions", params: { subscription: @valid.merge(endpoint: endpoint) }, headers: @headers, as: :json
        assert_response :unprocessable_entity
      end
      post "/api/push_subscriptions", params: { subscription: @valid.merge(keys: { p256dh: "bad", auth: "bad" }) }, headers: @headers, as: :json
      assert_response :unprocessable_entity
      invalid_point = Base64.urlsafe_encode64("\x04".b + "\0".b * 64, padding: false)
      post "/api/push_subscriptions", params: { subscription: @valid.merge(keys: @valid[:keys].merge(p256dh: invalid_point)) }, headers: @headers, as: :json
      assert_response :unprocessable_entity
      assert_empty @messenger.push_subscriptions
    end

    test "explicit browser account switch requires both existing encryption keys" do
      subscription = browser_subscription(@other)
      body = { endpoint: subscription.endpoint, keys: { p256dh: @valid[:keys][:p256dh], auth: @valid[:keys][:auth] } }
      post "/api/push_subscriptions", params: { subscription: body }, headers: @headers, as: :json
      assert_response :unprocessable_entity
      assert_equal @other.id, subscription.reload.user_id
      body[:keys] = { p256dh: subscription.p256dh, auth: subscription.auth }
      post "/api/push_subscriptions", params: { subscription: body }, headers: @headers, as: :json
      assert_response :created
      assert_equal @messenger.id, subscription.reload.user_id
    end

    test "missing configuration fails gracefully and the test only addresses this account browser" do
      ENV["WEB_PUSH_PRIVATE_KEY"] = nil
      get "/api/push_config", headers: @headers
      assert_equal false, response.parsed_body["configured"]
      post "/api/push_subscriptions", params: { subscription: @valid }, headers: @headers, as: :json
      assert_response :service_unavailable
      ENV["WEB_PUSH_PRIVATE_KEY"] = "configured-for-stubbed-transport"
      subscription = browser_subscription(@messenger)
      sent = []
      with_method(WebPushTransport, :deliver, ->(target, payload) { sent << [ target.id, payload ]; :sent }) do
        post "/api/push_subscriptions/test", params: { endpoint: subscription.endpoint }, headers: @headers, as: :json
        assert_response :success
      end
      assert_equal [ subscription.id ], sent.map(&:first)
      assert_equal "/history", sent.first.last[:url]
    end
  end
end
