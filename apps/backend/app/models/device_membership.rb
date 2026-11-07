class DeviceMembership < ApplicationRecord
  ROLES = %w[resident messenger].freeze

  belongs_to :user
  belongs_to :device

  validates :role, inclusion: { in: ROLES }
end
