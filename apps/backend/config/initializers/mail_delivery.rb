# SMTP credentials stay in the host environment. Tests keep the :test delivery method.
unless Rails.env.test?
  if ENV["SMTP_ADDRESS"].present?
    ActionMailer::Base.delivery_method = :smtp
    ActionMailer::Base.raise_delivery_errors = true
    ActionMailer::Base.smtp_settings = {
      address: ENV["SMTP_ADDRESS"],
      port: ENV.fetch("SMTP_PORT", "587").to_i,
      user_name: ENV["SMTP_USER_NAME"].presence,
      password: ENV["SMTP_PASSWORD"].presence,
      authentication: :plain,
      enable_starttls_auto: true
    }
  else
    ActionMailer::Base.perform_deliveries = false
  end
end
