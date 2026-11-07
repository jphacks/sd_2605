class DeviceAudio < ApplicationRecord
  belongs_to :device
  belongs_to :user, optional: true
  validates :slot, inclusion: { in: %w[leave arrive] }, uniqueness: { scope: :device_id }
  validates :wav, presence: true, length: { maximum: 200 * 1024 }
end
