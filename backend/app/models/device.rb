class Device < ApplicationRecord
  has_one :device_setting, dependent: :destroy
  has_many :home_events, dependent: :destroy
end
