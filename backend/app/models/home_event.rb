class HomeEvent < ApplicationRecord
  EVENT_TYPES = %w[left_home arrived_home].freeze

  belongs_to :device

  validates :event_type, inclusion: { in: EVENT_TYPES }
  validates :occurred_at, presence: true
end
