class WebPushSettings
  def self.configured?
    public_key.present? && ENV["WEB_PUSH_PRIVATE_KEY"].present? && subject.present?
  end

  def self.public_key
    ENV["WEB_PUSH_PUBLIC_KEY"]
  end

  def self.subject
    ENV["WEB_PUSH_SUBJECT"]
  end

  def self.vapid
    { subject: subject, public_key: public_key, private_key: ENV["WEB_PUSH_PRIVATE_KEY"] }
  end
end
