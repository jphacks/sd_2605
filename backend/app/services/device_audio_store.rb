# PostgreSQL bytea survives Render's ephemeral filesystem and keeps bytes/version atomic.
class DeviceAudioStore
  SLOTS = %w[leave arrive].freeze

  def self.bytes(device, slot)
    device.device_audios.find_by(slot: slot)&.wav || File.binread(Rails.root.join("config", "audio", "#{slot}.wav"))
  end

  def self.metadata(device, setting, slot)
    audio = device.device_audios.find_by(slot: slot)
    {
      version: setting.public_send("#{slot}_audio_version"),
      byte_size: audio ? audio.wav.bytesize : File.size(Rails.root.join("config", "audio", "#{slot}.wav")),
      content_type: "audio/wav",
      path: "/api/devices/#{device.id}/audio/#{slot}",
      file_name: "#{slot}.wav",
      updated_at: audio&.updated_at&.iso8601,
      registered_by: audio&.registered_by
    }
  end

  def self.write!(device, setting, slot, bytes, sender)
    setting.with_lock do
      audio = device.device_audios.find_or_initialize_by(slot: slot)
      audio.update!(wav: bytes, registered_by: sender)
      setting.public_send("#{slot}_audio_version=", setting.public_send("#{slot}_audio_version") + 1)
      setting.message_registered_by = sender
      setting.message_uploaded_at = Time.current
      setting.save!
    end
  end
end
