class Device < ApplicationRecord
  has_secure_password validations: false

  belongs_to :user, optional: true
  has_one :device_setting, dependent: :destroy
  has_many :home_events, dependent: :destroy

  validates :id, presence: true, format: { with: /\A[A-Za-z0-9_-]{1,64}\z/ }
  validates :password, presence: true, on: :create
  validates :password, length: { minimum: 8 }, if: -> { password.present? }
end
