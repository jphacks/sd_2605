class HomeEventNotice
  # Returns false only when every messenger delivery failed, so the caller can allow a retry.
  def self.deliver!(event, wav: nil)
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
