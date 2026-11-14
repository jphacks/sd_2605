class HomeEventNotice
  # Mail is opt-in only. Push uses a persistent outbox so provider waits don't delay the WAV API.
  def self.deliver!(event, wav: nil)
    WebPushNotice.enqueue!(event)
    return true unless ENV.fetch("MAIL_NOTIFICATIONS_ENABLED", "false") == "true"

    memberships = event.device.device_memberships.where(role: "messenger").includes(:user).to_a
    return true if memberships.empty?

    sent = false
    memberships.each do |membership|
      HomeEventMailer.with(user: membership.user, event: event, wav: wav).messenger_notice.deliver_now
      sent = true
    rescue StandardError => error
      Rails.logger.error("home event mail failed for event #{event.id}: #{error.class}")
    end
    sent
  end
end
