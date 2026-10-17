class DeviceAudio < ApplicationRecord
  belongs_to :device
  validates :slot, inclusion: { in: %w[leave arrive] }, uniqueness: { scope: :device_id }
  validates :wav, presence: true, length: { maximum: 200 * 1024 }
end
