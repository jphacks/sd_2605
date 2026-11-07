class HomeEvent < ApplicationRecord
  enum :event_type, { left_home: "left_home", arrived_home: "arrived_home" }, validate: true

  belongs_to :device
  has_one :home_event_voice, dependent: :destroy

  validates :id, presence: true
  validates :occurred_at, presence: true
end
