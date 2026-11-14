class WebPushTransport
  def self.deliver(subscription, payload)
    return :failed unless WebPushSettings.configured?

    WebPush.payload_send(
      message: JSON.generate(payload), endpoint: subscription.endpoint,
      p256dh: subscription.p256dh, auth: subscription.auth, vapid: WebPushSettings.vapid,
      ttl: 3600, urgency: "normal", ssl_timeout: 3, open_timeout: 3, read_timeout: 5
    )
    :sent
  rescue WebPush::ExpiredSubscription, WebPush::InvalidSubscription
    subscription.destroy!
    :expired
  rescue StandardError => error
    # Never log endpoint URLs, encryption keys, payloads or credential values.
    Rails.logger.warn("web push failed for subscription #{subscription.id}: #{error.class}")
    :failed
  end
end
