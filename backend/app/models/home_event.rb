class HomeEvent < ApplicationRecord
  enum :event_type, { left_home: "left_home", arrived_home: "arrived_home" }, validate: true

  belongs_to :device

  validates :occurred_at, presence: true
end
