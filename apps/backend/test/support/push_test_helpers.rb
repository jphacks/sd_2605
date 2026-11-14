module PushTestHelpers
  def configure_push
    @saved_push_env = %w[WEB_PUSH_PUBLIC_KEY WEB_PUSH_PRIVATE_KEY WEB_PUSH_SUBJECT MAIL_NOTIFICATIONS_ENABLED].to_h { |key| [ key, ENV[key] ] }
    key = WebPush.generate_key
    ENV["WEB_PUSH_PUBLIC_KEY"] = key.public_key
    ENV["WEB_PUSH_PRIVATE_KEY"] = key.private_key
    ENV["WEB_PUSH_SUBJECT"] = "https://example.com"
    ENV["MAIL_NOTIFICATIONS_ENABLED"] = "false"
  end

  def restore_push
    @saved_push_env.each { |key, value| ENV[key] = value }
  end

  def browser_subscription(user)
    user.push_subscriptions.create!(endpoint: "https://web.push.apple.com/test-#{SecureRandom.hex(8)}",
      p256dh: WebPush.generate_key.public_key, auth: Base64.urlsafe_encode64(SecureRandom.random_bytes(16), padding: false))
  end

  def with_method(target, name, replacement)
    original = target.method(name)
    target.singleton_class.define_method(name, replacement)
    yield
  ensure
    target.singleton_class.define_method(name, original)
  end
end
