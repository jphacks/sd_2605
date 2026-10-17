require "test_helper"
require "stringio"

module Api
  class DeviceApiTest < ActionDispatch::IntegrationTest
    setup do
      @device = Device.create!(id: "api-#{SecureRandom.hex(4)}", password: "test-password")
      @other_device = Device.create!(id: "other-#{SecureRandom.hex(4)}", password: "test-password")
      @device.device_setting || @device.create_device_setting!
      post "/api/sessions", params: { device_id: @device.id, password: "test-password" }, as: :json
      @token = response.parsed_body.fetch("token")
      @headers = { "Authorization" => "Bearer #{@token}" }
    end

    teardown do
      path = MessageAudioStore.path_for(@device) if @device
      File.delete(path) if path&.file?
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

    test "rejects a token when it is used for another device" do
      get "/api/devices/#{@other_device.id}/status", headers: @headers

      assert_response :unauthorized
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

    test "stores a validated wav on disk and increments the database version" do
      post "/api/devices/#{@device.id}/message",
        params: { file: Rack::Test::UploadedFile.new(StringIO.new(wav_bytes), "audio/wav", true), sender_name: "Family" },
        headers: @headers

      assert_response :created
      setting = @device.device_setting.reload
      assert_equal 2, setting.arrive_audio_version
      assert_equal "Family", setting.message_registered_by
      assert MessageAudioStore.path_for(@device).file?

      get "/api/devices/#{@device.id}/status", headers: @headers
      assert_equal 1, response.parsed_body["message_count"]
      assert_equal 2, response.parsed_body.dig("message_audio", "version")
    end

    test "rejects invalid wav data" do
      post "/api/devices/#{@device.id}/message",
        params: { file: Rack::Test::UploadedFile.new(StringIO.new("not wav"), "audio/wav", true), sender_name: "Family" },
        headers: @headers

      assert_response :unprocessable_entity
      assert_equal 1, @device.device_setting.reload.arrive_audio_version
    end

    test "updates last seen time on heartbeat" do
      post "/api/devices/#{@device.id}/heartbeat", headers: @headers

      assert_response :no_content
      assert @device.reload.online?
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
