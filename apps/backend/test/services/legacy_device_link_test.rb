require "test_helper"
require_relative "../../db/migrate/20261018001000_link_legacy_entrance_device_to_account"

class LegacyDeviceLinkTest < ActionDispatch::IntegrationTest
  setup do
    @user = User.create!(email: "legacy-owner@example.com", password: "test-password")
    @device = Device.create!(id: "entrance-1", password: "device-password")
    @device.update_column(:password_digest, nil)
    @repair = LinkLegacyEntranceDeviceToAccount.new
    digest = Digest::SHA256.hexdigest(@user.email)
    @repair.define_singleton_method(:target_account_digest) { digest }
  end

  test "links only the designated account and preserves settings and audio" do
    other = User.create!(email: "other@example.com", password: "test-password")
    setting = @device.create_device_setting!(volume: 35, arrive_audio_version: 4)
    bytes = File.binread(Rails.root.join("config", "audio", "arrive.wav"))
    audio = @device.device_audios.create!(slot: "arrive", wav: bytes)
    @repair.up
    assert_equal @user.id, @device.reload.user_id
    assert_not_equal other.id, @device.user_id
    assert_nil @device.password_digest
    assert_equal 35, setting.reload.volume
    assert_equal 4, setting.arrive_audio_version
    assert_equal bytes, audio.reload.wav
    @repair.up
    assert_equal @user.id, @device.reload.user_id
    headers = { "Authorization" => "Bearer #{JsonWebToken.encode(@user.id).first}" }
    get "/api/devices/entrance-1/status", headers: headers
    assert_response :ok
  end

  test "never transfers an already owned device" do
    other = User.create!(email: "another-owner@example.com", password: "test-password")
    @device.update!(user: other)
    @repair.up
    assert_equal other.id, @device.reload.user_id
  end

  test "never bypasses existing device credentials" do
    @device.update!(password: "configured-password")
    @repair.up
    assert_nil @device.reload.user_id
    assert @device.authenticate("configured-password")
  end

  test "does nothing when the designated user does not exist" do
    @repair.define_singleton_method(:target_account_digest) { "missing" }
    @repair.up
    assert_nil @device.reload.user_id
  end
end
