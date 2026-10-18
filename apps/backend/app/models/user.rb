class User < ApplicationRecord
  has_many :device_memberships, dependent: :destroy
  has_many :devices, through: :device_memberships
  has_many :registered_devices, class_name: "Device", dependent: :restrict_with_error

  has_secure_password validations: false

  normalizes :email, with: ->(email) { email.strip.downcase }

  validates :email, presence: true, uniqueness: true, format: { with: URI::MailTo::EMAIL_REGEXP }
  validates :google_sub, uniqueness: true, allow_nil: true, length: { maximum: 255 }
  validates :password, presence: true, on: :create, unless: :google_account?
  validates :password,
    length: { minimum: 8, maximum: ActiveModel::SecurePassword::MAX_PASSWORD_LENGTH_ALLOWED },
    if: -> { password.present? }

  def self.from_google_identity(payload)
    return nil unless payload.is_a?(Hash)

    sub = payload["sub"].to_s
    email = payload["email"].to_s.strip.downcase
    return nil unless google_identity_acceptable?(sub, email, payload["email_verified"])

    transaction do
      linked = lock.find_by(google_sub: sub)
      return linked if linked

      user = lock.find_by(email: email)
      if user
        return nil if user.google_sub.present?

        user.update!(google_sub: sub)
        return user
      end

      create!(email: email, google_sub: sub)
    end
  rescue ActiveRecord::RecordNotUnique, ActiveRecord::RecordInvalid
    linked = find_by(google_sub: sub)
    linked if linked&.google_sub == sub
  end

  def self.google_identity_acceptable?(sub, email, email_verified)
    email_verified == true && sub.present? && sub.length <= 255 && email.match?(URI::MailTo::EMAIL_REGEXP)
  end
  private_class_method :google_identity_acceptable?

  def google_account?
    google_sub.present?
  end
end
