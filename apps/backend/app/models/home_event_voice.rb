class HomeEventVoice < ApplicationRecord
  belongs_to :home_event

  validates :wav, presence: true, length: { maximum: WavAudio::MAX_BYTES }
end
