require "test_helper"

class DeviceAudioStoreTest < ActiveSupport::TestCase
  test "a failed settings commit rolls back audio bytes and version together" do
    device = Device.create!(id: "atomic-audio", password: "test-password")
    setting = device.create_device_setting!
    bytes = File.binread(Rails.root.join("config", "audio", "arrive.wav"))
    DeviceAudioStore.write!(device, setting, "arrive", bytes, "First")
    setting.define_singleton_method(:save!) { raise ActiveRecord::RecordInvalid, self }
    assert_raises(ActiveRecord::RecordInvalid) do
      DeviceAudioStore.write!(device, setting, "arrive", bytes + "broken", "Second")
    end
    assert_equal 2, DeviceSetting.find(setting.id).arrive_audio_version
    audio = device.device_audios.find_by!(slot: "arrive")
    assert_equal bytes, audio.wav
    assert_equal "First", audio.registered_by
  end
end
