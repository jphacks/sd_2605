class PushDelivery < ApplicationRecord
  MAX_ATTEMPTS = 5
  belongs_to :home_event
  belongs_to :push_subscription

  scope :due, -> {
    where(sent_at: nil, canceled_at: nil).where("attempts < ?", MAX_ATTEMPTS)
      .where("next_attempt_at IS NULL OR next_attempt_at <= ?", Time.current)
  }
end
