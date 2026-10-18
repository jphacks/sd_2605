require "test_helper"
require "stringio"

module Api
  class DeviceApiTest < ActionDispatch::IntegrationTest
    setup do
      @user = User.create!(email: "api-#{SecureRandom.hex(4)}@example.com", password: "test-password")
      @device = Device.create!(id: "api-#{SecureRandom.hex(4)}", user: @user, password: "test-password")
      @other_device = Device.create!(id: "other-#{SecureRandom.hex(4)}", password: "test-password")
      @device.create_device_setting!
      @headers = { "Authorization" => "Bearer #{JsonWebToken.encode(@user.id).first}" }
    end

    test "returns device status and settings from the database" do
      get "/api/devices/#{@device.id}/status", headers: @headers
      assert_response :success
      assert_equal @device.id, response.parsed_body["device_id"]
      assert_equal false, response.parsed_body["online"]

      get "/api/devices/#{@device.id}/settings", headers: @headers
      assert_response :success
      assert_equal 70, response.parsed_body["volume"]
    end

    test "lets another member read history, change settings, and upload audio" do
      other = User.create!(email: "member-#{SecureRandom.hex(4)}@example.com", password: "password1")
      headers = { "Authorization" => "Bearer #{JsonWebToken.encode(other.id).first}" }
      HomeEvent.create!(
        id: "shared-event",
        device: @device,
        event_type: "arrived_home",
        occurred_at: Time.utc(2026, 10, 17, 8, 0, 0)
      )

      post "/api/sessions",
        params: { device_id: @device.id, password: "test-password" },
        as: :json,
        headers: headers
      assert_response :success
      assert_equal @user, @device.reload.user

      get "/api/devices/#{@device.id}/events", headers: headers
      assert_response :success
      assert_equal [ "shared-event" ], response.parsed_body.map { |event| event["event_id"] }

      patch "/api/devices/#{@device.id}/settings", params: { volume: 40 }, as: :json, headers: headers
      assert_response :success
      assert_equal 40, @device.device_setting.reload.volume

      post "/api/devices/#{@device.id}/message",
        params: {
          file: Rack::Test::UploadedFile.new(StringIO.new(wav_bytes), "audio/wav", true, original_filename: "voice.wav"),
          sender_name: "Other"
        },
        headers: headers
      assert_response :created
      assert_equal "Other", @device.device_setting.reload.message_registered_by
      assert_equal wav_bytes, @device.device_audios.find_by!(slot: "arrive").wav
    end

    test "returns forbidden rather than expiring a valid login for another device" do
      get "/api/devices/#{@other_device.id}/status", headers: @headers

      assert_response :forbidden
      assert_equal "device_forbidden", response.parsed_body.dig("error", "code")
    end

    test "login survives the initial default device access when it is not owned" do
      post "/api/user_sessions", params: { email: @user.email, password: "test-password" }, as: :json
      assert_response :ok
      headers = { "Authorization" => "Bearer #{response.parsed_body.fetch('token')}" }

      get "/api/devices/#{@other_device.id}/status", headers: headers
      assert_response :forbidden
      get "/api/devices/#{@other_device.id}/events", headers: headers
      assert_response :forbidden
      get "/api/devices/#{@device.id}/status", headers: headers
      assert_response :ok
      post "/api/devices", params: { device_id: "new-after-login", password: "test-password" }, as: :json, headers: headers
      assert_response :created
    end

    test "updates the persisted volume and rejects out of range values" do
      patch "/api/devices/#{@device.id}/settings", params: { volume: 0 }, as: :json, headers: @headers
      assert_response :success
      assert_equal 0, @device.device_setting.reload.volume

      patch "/api/devices/#{@device.id}/settings", params: { volume: 101 }, as: :json, headers: @headers
      assert_response :unprocessable_entity
      assert_equal 0, @device.device_setting.reload.volume
    end

    test "stores events idempotently and lists them newest first" do
      params = { event_id: "event-1", event_type: "left_home", occurred_at: "2026-10-17T08:00:00Z" }
      post "/api/devices/#{@device.id}/events", params: params, as: :json, headers: @headers
      assert_response :created
      assert_equal "event-1", response.parsed_body["event_id"]

      post "/api/devices/#{@device.id}/events", params: params.merge(occurred_at: "2026-10-17T09:00:00Z"), as: :json, headers: @headers
      assert_response :ok
      assert_equal "2026-10-17T08:00:00Z", response.parsed_body["occurred_at"]
      assert_equal 1, @device.home_events.count

      get "/api/devices/#{@device.id}/events", headers: @headers
      assert_response :success
      assert_equal [ "event-1" ], response.parsed_body.map { |event| event["event_id"] }
    end

    test "stores a validated wav in PostgreSQL and increments the database version" do
      post "/api/devices/#{@device.id}/message",
        params: { file: Rack::Test::UploadedFile.new(StringIO.new(wav_bytes), "audio/wav", true, original_filename: "voice.wav"), sender_name: "Family" },
        headers: @headers

      assert_response :created
      setting = @device.device_setting.reload
      assert_equal 2, setting.arrive_audio_version
      assert_equal "Family", setting.message_registered_by
      assert_equal wav_bytes, @device.device_audios.find_by!(slot: "arrive").wav

      get "/api/devices/#{@device.id}/status", headers: @headers
      assert_equal 1, response.parsed_body["message_count"]
      assert_equal 2, response.parsed_body.dig("message_audio", "version")
      assert_equal 2, response.parsed_body.dig("arrive_audio", "version")
      assert_nil response.parsed_body["leave_audio"]
    end

    test "rejects invalid wav data" do
      post "/api/devices/#{@device.id}/message",
        params: { file: Rack::Test::UploadedFile.new(StringIO.new("not wav"), "audio/wav", true, original_filename: "voice.wav"), sender_name: "Family" },
        headers: @headers

      assert_response :unprocessable_entity
      assert_equal 1, @device.device_setting.reload.arrive_audio_version
    end

    test "updates last seen time on heartbeat" do
      post "/api/devices/#{@device.id}/heartbeat", headers: @headers

      assert_response :ok
      assert @device.reload.online?
      assert_equal @device.id, response.parsed_body["device_id"]
      assert_equal "audio/wav", response.parsed_body.dig("arrive_audio", "content_type")
      assert_equal 1, response.parsed_body.dig("leave_audio", "version")
    end

    test "hardware heartbeat and WAV GET work without browser authentication" do
      post "/api/devices/#{@device.id}/heartbeat", params: {}, as: :json
      assert_response :ok
      body = response.parsed_body
      %w[leave arrive].each do |slot|
        meta = body.fetch("#{slot}_audio")
        assert_equal 1, meta["version"]
        assert_equal "/api/devices/#{@device.id}/audio/#{slot}", meta["path"]
        get meta["path"]
        assert_response :ok
        assert_equal "audio/wav", response.media_type
        assert_equal meta["byte_size"], response.body.bytesize
        assert_equal meta["byte_size"].to_s, response.headers["Content-Length"]
        assert WavAudio.valid?(response.body)
        assert_nil response.headers["Location"]
      end
    end

    test "upload then heartbeat then download returns the exact bytes and only updates arrive" do
      bytes = wav_bytes
      post "/api/devices/#{@device.id}/message",
        params: { file: Rack::Test::UploadedFile.new(StringIO.new(bytes), "audio/wav", true, original_filename: "voice.wav"), sender_name: "Family" },
        headers: @headers
      assert_response :created
      post "/api/devices/#{@device.id}/heartbeat", params: {}, as: :json
      assert_response :ok
      assert_equal 2, response.parsed_body.dig("arrive_audio", "version")
      assert_equal 1, response.parsed_body.dig("leave_audio", "version")
      get response.parsed_body.dig("arrive_audio", "path")
      assert_response :ok
      assert_equal bytes, response.body.b
      assert_equal bytes, Device.find(@device.id).device_audios.find_by!(slot: "arrive").wav
    end

    test "status lists leave and arrive only after each slot is registered" do
      get "/api/devices/#{@device.id}/status", headers: @headers
      assert_nil response.parsed_body["leave_audio"]
      assert_nil response.parsed_body["arrive_audio"]

      post "/api/devices/#{@device.id}/message",
        params: { slot: "leave", file: Rack::Test::UploadedFile.new(StringIO.new(wav_bytes), "audio/wav", true, original_filename: "voice.wav"), sender_name: "Family" },
        headers: @headers
      assert_response :created
      get "/api/devices/#{@device.id}/status", headers: @headers
      assert_equal 2, response.parsed_body.dig("leave_audio", "version")
      assert_equal "Family", response.parsed_body.dig("leave_audio", "registered_by")
      assert_nil response.parsed_body["arrive_audio"]
      assert_nil response.parsed_body["message_audio"]
    end

    test "a leave upload does not replace arrive" do
      post "/api/devices/#{@device.id}/message",
        params: { slot: "leave", file: Rack::Test::UploadedFile.new(StringIO.new(wav_bytes), "audio/wav", true, original_filename: "voice.wav") },
        headers: @headers
      assert_response :created
      assert_equal 2, @device.device_setting.reload.leave_audio_version
      assert_equal 1, @device.device_setting.arrive_audio_version
    end

    test "invalid replacement preserves the previously uploaded bytes and version" do
      DeviceAudioStore.write!(@device, @device.device_setting, "arrive", wav_bytes, "Family")
      post "/api/devices/#{@device.id}/message",
        params: { file: Rack::Test::UploadedFile.new(StringIO.new("broken"), "audio/wav", true, original_filename: "voice.wav") }, headers: @headers
      assert_response :unprocessable_entity
      assert_equal 2, @device.device_setting.reload.arrive_audio_version
      get "/api/devices/#{@device.id}/audio/arrive"
      assert_equal wav_bytes, response.body.b
    end

    test "hardware event keeps its ID and timestamp when retried without authentication" do
      payload = { event_id: "event-hardware", event_type: "arrived_home", occurred_at: "2026-10-17T08:00:00Z" }
      post "/api/devices/#{@device.id}/events", params: payload, as: :json
      assert_response :created
      post "/api/devices/#{@device.id}/events", params: payload, as: :json
      assert_response :ok
      assert_equal payload.stringify_keys, response.parsed_body
      assert_equal 1, @device.home_events.count
      post "/api/devices/#{@device.id}/events", params: payload.merge(event_type: "bad"), as: :json
      assert_response :unprocessable_entity
    end

    test "unknown device or slot returns JSON 404 and web writes require authentication" do
      post "/api/devices/missing/heartbeat", params: {}, as: :json
      assert_response :not_found
      assert_equal "not_found", response.parsed_body.dig("error", "code")
      get "/api/devices/#{@device.id}/audio/unknown"
      assert_response :not_found
      post "/api/devices/#{@device.id}/message"
      assert_response :unauthorized
      patch "/api/devices/#{@device.id}/settings", params: { volume: 0 }, as: :json
      assert_response :unauthorized
    end

    private

    def wav_bytes
      samples = "\x00\x00".b * 16000
      bytes = "RIFF".b
      bytes << [ 36 + samples.bytesize ].pack("V")
      bytes << "WAVEfmt ".b
      bytes << [ 16, 1, 1, 16000, 32000, 2, 16 ].pack("VvvVVvv")
      bytes << "data".b
      bytes << [ samples.bytesize ].pack("V")
      bytes << samples
    end
  end
end
