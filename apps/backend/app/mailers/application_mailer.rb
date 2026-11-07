class ApplicationMailer < ActionMailer::Base
  default from: ENV.fetch("MAIL_FROM", "おかえりんく <noreply@example.com>")
  layout "mailer"
end
