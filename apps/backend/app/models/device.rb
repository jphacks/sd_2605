class Device < ApplicationRecord
  has_secure_password validations: false

  belongs_to :user, optional: true
  has_one :device_setting, dependent: :destroy
  has_many :home_events, dependent: :destroy
  has_many :device_audios, dependent: :destroy
  has_many :device_memberships, dependent: :destroy
  has_many :device_invitations, dependent: :destroy
  has_many :members, through: :device_memberships, source: :user

  after_save :grant_access_to_registering_user, if: :saved_change_to_user_id?

  def online?
    last_seen_at.present? && last_seen_at > 60.seconds.ago
  end

  def accessible_to?(user)
    user.present? && (user_id == user.id || members.exists?(id: user.id))
  end

  def resident?(user)
    user.present? && (user_id == user.id || device_memberships.exists?(user: user, role: "resident"))
  end

  def messenger?(user)
    user.present? && device_memberships.exists?(user: user, role: "messenger")
  end

  # Membership is the role the list API already returns. The registrant stays a resident
  # even when that membership row has not been backfilled.
  def role_for(user)
    return unless user

    device_memberships.find_by(user: user)&.role || ("resident" if resident?(user))
  end

  # Rejoining keeps the existing role. The given role applies only to a new membership.
  def grant_access_to!(user, role: "resident")
    device_memberships.find_or_create_by!(user: user) { |membership| membership.role = role }
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
