class User < ApplicationRecord
  has_many :devices, dependent: :restrict_with_error

  has_secure_password

  normalizes :email, with: ->(email) { email.strip.downcase }

  validates :email, presence: true, uniqueness: true, format: { with: URI::MailTo::EMAIL_REGEXP }
  validates :password, length: { minimum: 8 }, if: -> { password.present? }
end
