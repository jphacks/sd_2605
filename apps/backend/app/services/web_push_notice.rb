require "uri"

class WebPushNotice
  def self.enqueue!(event)
    recipients = event.device.device_memberships.where(role: "messenger").select(:user_id)
    PushSubscription.where(user_id: recipients).find_each do |subscription|
      delivery = PushDelivery.find_or_create_by!(home_event: event, push_subscription: subscription)
      schedule(delivery.id) unless delivery.sent_at || delivery.canceled_at
    end
    true
  end

  def self.recover_pending!(device)
    PushDelivery.due.joins(:home_event).where(home_events: { device_id: device.id }).limit(20).pluck(:id).each { |id| schedule(id) }
  end

  def self.schedule(id)
    PushDeliveryJob.perform_later(id)
  rescue ActiveJob::EnqueueError => error
    Rails.logger.warn("web push scheduling failed for delivery #{id}: #{error.class}")
    # The durable outbox is recovered by the device heartbeat after a restart/failure.
  end

  def self.payload(event)
    {
      title: event.left_home? ? "お出かけしました" : "帰宅しました",
      body: event.home_event_voice.present? ? "玄関から声の返信が届きました。タップして確認できます。" : "玄関から新しいお知らせが届きました。",
      tag: "home-event-#{event.id}",
      url: "/history?#{URI.encode_www_form(device_id: event.device_id, event_id: event.id)}"
    }
  end
end
