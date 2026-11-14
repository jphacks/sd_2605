require "test_helper"
require "stringio"

module Api
  class DeviceCharacterTest < ActionDispatch::IntegrationTest
    setup do
      @user = User.create!(email: "face-#{SecureRandom.hex(4)}@example.com", password: "test-password")
      @device = Device.create!(id: "face-#{SecureRandom.hex(4)}", user: @user, password: "test-password")
      @device.create_device_setting!
      @headers = { "Authorization" => "Bearer #{JsonWebToken.encode(@user.id).first}" }
      @previous_key = ENV["GEMINI_API_KEY"]
      ENV.delete("GEMINI_API_KEY")
    end

    teardown do
      if @previous_key
        ENV["GEMINI_API_KEY"] = @previous_key
      else
        ENV.delete("GEMINI_API_KEY")
      end
    end

    test "an unsaved face is absent from GET and heartbeat offers version 0" do
      get "/api/devices/#{@device.id}/character"
      assert_response :not_found
      assert_equal "not_found", response.parsed_body.dig("error", "code")

      post "/api/devices/#{@device.id}/heartbeat", params: {}, as: :json
      assert_response :ok
      assert_equal 0, response.parsed_body.dig("character", "version")
      assert_equal "/api/devices/#{@device.id}/character", response.parsed_body.dig("character", "path")
    end

    test "a resident saves a catalog face and the version increases" do
      put "/api/devices/#{@device.id}/character", params: face, as: :json, headers: @headers
      assert_response :ok
      assert_equal "round", response.parsed_body["glasses"]
      assert_equal [ "freckles" ], response.parsed_body["extras"]
      assert_equal 1, @device.device_setting.reload.character_version

      get "/api/devices/#{@device.id}/character"
      assert_response :ok
      assert_equal "round", response.parsed_body["glasses"]

      put "/api/devices/#{@device.id}/character", params: face.merge("mouth" => "w"), as: :json, headers: @headers
      assert_response :ok
      assert_equal 2, @device.device_setting.reload.character_version
      post "/api/devices/#{@device.id}/heartbeat", params: {}, as: :json
      assert_equal 2, response.parsed_body.dig("character", "version")
    end

    test "a save rejects parts outside the catalog and leaves the stored face alone" do
      put "/api/devices/#{@device.id}/character", params: face.merge("glasses" => "monocle"), as: :json, headers: @headers
      assert_response :unprocessable_entity
      assert_equal "validation_error", response.parsed_body.dig("error", "code")
      assert_nil @device.device_setting.reload.character
      assert_equal 0, @device.device_setting.character_version
    end

    test "a messenger can read a saved face but cannot change it" do
      @device.device_setting.update!(character: face, character_version: 1)
      messenger = User.create!(email: "messenger-#{SecureRandom.hex(4)}@example.com", password: "password1")
      @device.grant_access_to!(messenger, role: "messenger")
      headers = { "Authorization" => "Bearer #{JsonWebToken.encode(messenger.id).first}" }

      get "/api/devices/#{@device.id}/character", headers: headers
      assert_response :ok
      assert_equal "pink", response.parsed_body["bg"]

      put "/api/devices/#{@device.id}/character", params: face, as: :json, headers: headers
      assert_response :forbidden
      assert_equal "resident_only", response.parsed_body.dig("error", "code")

      post "/api/devices/#{@device.id}/character/extract",
        params: { file: upload(png_bytes, "image/png", "face.png") },
        headers: headers
      assert_response :forbidden
      assert_equal 1, @device.device_setting.reload.character_version
    end

    test "extract returns a suggestion and does not store the photo or the face" do
      suggestion = face.merge("eyes" => "lashed")
      called = nil
      CharacterExtractor.stub(:call, ->(bytes, mime) { called = [ bytes, mime ]; suggestion }) do
        post "/api/devices/#{@device.id}/character/extract",
          params: { file: upload(png_bytes, "image/png", "face.png") },
          headers: @headers
      end

      assert_response :ok
      assert_equal "lashed", response.parsed_body["eyes"]
      assert_equal [ png_bytes, "image/png" ], called
      setting = @device.device_setting.reload
      assert_nil setting.character
      assert_equal 0, setting.character_version
      assert_equal 0, @device.device_audios.count
    end

    test "extract rejects a file that is not a photo and stores nothing" do
      post "/api/devices/#{@device.id}/character/extract",
        params: { file: upload("hello", "text/plain", "note.txt") },
        headers: @headers

      assert_response :unprocessable_entity
      assert_nil @device.device_setting.reload.character
      assert_equal 0, @device.device_setting.character_version
    end

    test "extract reports an upstream failure instead of saving a default face" do
      ENV.delete("GEMINI_API_KEY")
      post "/api/devices/#{@device.id}/character/extract",
        params: { file: upload(png_bytes, "image/png", "face.png") },
        headers: @headers

      assert_response :service_unavailable
      assert_equal "character_unavailable", response.parsed_body.dig("error", "code")
      assert_nil @device.device_setting.reload.character
    end

    test "a broken login cannot read a saved face" do
      @device.device_setting.update!(character: face, character_version: 1)

      get "/api/devices/#{@device.id}/character", headers: { "Authorization" => "Bearer not-a-token" }

      assert_response :unauthorized
      assert_equal "invalid_credentials", response.parsed_body.dig("error", "code")
    end

    test "a login for another home cannot read this face" do
      other = User.create!(email: "other-#{SecureRandom.hex(4)}@example.com", password: "password1")
      headers = { "Authorization" => "Bearer #{JsonWebToken.encode(other.id).first}" }
      @device.device_setting.update!(character: face, character_version: 1)

      get "/api/devices/#{@device.id}/character", headers: headers
      assert_response :forbidden
      assert_equal "device_forbidden", response.parsed_body.dig("error", "code")
    end

    private

    def face
      {
        "bg" => "pink",
        "hairColor" => "black",
        "hair" => "straight-bangs",
        "eyeColor" => "brown",
        "eyes" => "female",
        "glasses" => "round",
        "beard" => "none",
        "mouth" => "closed-smile",
        "extras" => [ "freckles" ],
        "uncertain" => []
      }
    end

    def upload(bytes, type, name)
      Rack::Test::UploadedFile.new(StringIO.new(bytes), type, true, original_filename: name)
    end

    def png_bytes
      Base64.decode64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==")
    end
  end
end
