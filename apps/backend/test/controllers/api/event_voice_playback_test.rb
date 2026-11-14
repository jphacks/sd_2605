require "test_helper"

module Api
  class EventVoicePlaybackTest < ActionDispatch::IntegrationTest
    setup do
      @resident = User.create!(email: "play-resident-#{SecureRandom.hex(4)}@example.com", password: "password1")
      @messenger = User.create!(email: "play-messenger-#{SecureRandom.hex(4)}@example.com", password: "password1")
      @other = User.create!(email: "play-other-#{SecureRandom.hex(4)}@example.com", password: "password1")
      @device = Device.create!(id: "play-#{SecureRandom.hex(4)}", user: @resident, password: "device-pass-1")
      @device.grant_access_to!(@messenger, role: "messenger")
      @event = @device.home_events.create!(id: "play-event-#{SecureRandom.hex(8)}", event_type: "arrived_home", occurred_at: Time.current)
      @wav = File.binread(Rails.root.join("config", "audio", "arrive.wav"))
      @event.create_home_event_voice!(wav: @wav)
      @headers = { "Authorization" => "Bearer #{JsonWebToken.encode(@messenger.id).first}" }
      @path = "/api/devices/#{@device.id}/events/#{@event.id}/voice"
    end

    test "only authenticated members can download a private WAV" do
      get @path
      assert_response :unauthorized
      get @path, headers: { "Authorization" => "Bearer #{JsonWebToken.encode(@other.id).first}" }
      assert_response :forbidden
      get @path, headers: @headers
      assert_response :success
      assert_equal "audio/wav", response.media_type
      assert_equal @wav, response.body
      assert_includes response.headers["Cache-Control"], "no-store"
    end

    test "history and individual lookup identify recordings and missing voices return 404" do
      get "/api/devices/#{@device.id}/events", headers: @headers
      assert_equal true, response.parsed_body.first["voice_available"]
      get "/api/devices/#{@device.id}/events/#{@event.id}", headers: @headers
      assert_equal @event.id, response.parsed_body["event_id"]
      assert_equal true, response.parsed_body["voice_available"]
      @event.home_event_voice.destroy!
      get @path, headers: @headers
      assert_response :not_found
    end
  end
end
