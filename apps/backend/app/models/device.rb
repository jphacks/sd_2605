class Device < ApplicationRecord
  has_secure_password validations: false

  belongs_to :user, optional: true
  has_one :device_setting, dependent: :destroy
  has_many :home_events, dependent: :destroy
  has_many :device_audios, dependent: :destroy
  has_many :device_memberships, dependent: :destroy
  has_many :members, through: :device_memberships, source: :user

  after_save :grant_access_to_registering_user, if: :saved_change_to_user_id?

  def online?
    last_seen_at.present? && last_seen_at > 60.seconds.ago
  end

  def accessible_to?(user)
    user.present? && (user_id == user.id || members.exists?(id: user.id))
  end

  def grant_access_to!(user)
    device_memberships.find_or_create_by!(user: user)
  rescue ActiveRecord::RecordNotUnique
    device_memberships.find_by!(user: user)
  end

  validates :id, presence: true, format: { with: /\A[A-Za-z0-9_-]{1,64}\z/ }
  validates :password, presence: true, on: :create
  validates :password, length: { minimum: 8 }, if: -> { password.present? }

  private

  def grant_access_to_registering_user
    grant_access_to!(user) if user
  end
end
