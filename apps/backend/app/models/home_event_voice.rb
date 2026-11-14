class HomeEventVoice < ApplicationRecord
  belongs_to :home_event

  validates :wav, presence: true, length: { maximum: WavAudio::REPLY_MAX_BYTES }
end
