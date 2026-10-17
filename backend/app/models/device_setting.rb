class DeviceSetting < ApplicationRecord
  belongs_to :device

  validates :device_id, uniqueness: true
  validates :leave_greeting_enabled, :arrive_greeting_enabled, inclusion: { in: [ true, false ] }
  validates :volume,
    numericality: { only_integer: true, greater_than_or_equal_to: 0, less_than_or_equal_to: 100 }
  validates :leave_audio_version, :arrive_audio_version,
    numericality: { only_integer: true, greater_than_or_equal_to: 1 }
end
