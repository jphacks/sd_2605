require "test_helper"
require "stringio"

module Api
  class HomeEventVoiceTest < ActionDispatch::IntegrationTest
    setup do
      @previous_mail_flag = ENV["MAIL_NOTIFICATIONS_ENABLED"]
      ENV["MAIL_NOTIFICATIONS_ENABLED"] = "true"
      ActionMailer::Base.deliveries.clear
      @resident = User.create!(email: "voice-resident-#{SecureRandom.hex(4)}@example.com", password: "password1")
      @messenger = User.create!(email: "voice-messenger-#{SecureRandom.hex(4)}@example.com", password: "password1")
      @other = User.create!(email: "voice-other-#{SecureRandom.hex(4)}@example.com", password: "password1")
      @device = Device.create!(id: "voice-#{SecureRandom.hex(4)}", user: @resident, password: "device-pass-1")
      @device.grant_access_to!(@messenger, role: "messenger")
      @device.grant_access_to!(@other, role: "messenger")
    end

    teardown do
      ENV["MAIL_NOTIFICATIONS_ENABLED"] = @previous_mail_flag
    end

    test "mail notifications can be paused without stopping voice storage" do
      ENV["MAIL_NOTIFICATIONS_ENABLED"] = "false"
      post event_path, params: event_params, as: :json
      post voice_path, params: wav_bytes, headers: { "CONTENT_TYPE" => "audio/wav" }
      assert_response :created
      assert_equal wav_bytes, @device.home_events.find("event-1").home_event_voice.wav
      assert_empty ActionMailer::Base.deliveries
    end

    test "creating an event does not send mail until the voice is skipped or uploaded" do
      post event_path, params: event_params, as: :json

      assert_response :created
      assert_empty ActionMailer::Base.deliveries
    end

    test "a skipped recording emails every messenger once, without the resident or an attachment" do
      post event_path, params: event_params.merge(voice: "skipped"), as: :json
      assert_response :created
      assert_equal [ @messenger.email, @other.email ].sort, ActionMailer::Base.deliveries.map(&:to).flatten.sort
      mail = ActionMailer::Base.deliveries.first
      assert_equal "お出かけしました", mail.subject
      assert_empty mail.attachments
      assert_includes mail.body.encoded, "お出かけしました"

      ActionMailer::Base.deliveries.clear
      post event_path, params: event_params.merge(voice: "skipped", occurred_at: "2026-10-17T09:00:00Z"), as: :json
      assert_response :ok
      assert_empty ActionMailer::Base.deliveries
    end

    test "an uploaded wav is stored apart from greeting slots and mailed once" do
      post event_path, params: event_params("arrived_home"), as: :json
      assert_response :created

      post voice_path, params: { file: uploaded_wav }, headers: {}
      assert_response :created
      assert_equal wav_bytes, @device.home_events.find("event-1").home_event_voice.wav
      assert_empty @device.device_audios
      mails = ActionMailer::Base.deliveries
      assert_equal 2, mails.size
      assert_equal "帰宅しました", mails.first.subject
      assert_equal wav_bytes, mails.first.attachments["voice.wav"].decoded
      assert_not_includes mails.flat_map(&:to), @resident.email

      ActionMailer::Base.deliveries.clear
      post voice_path, params: { file: uploaded_wav }, headers: {}
      assert_response :ok
      assert_empty ActionMailer::Base.deliveries
    end

    test "a raw wav body is accepted" do
      post event_path, params: event_params, as: :json
      post voice_path, params: wav_bytes, headers: { "CONTENT_TYPE" => "audio/wav" }

      assert_response :created
      assert_equal wav_bytes, @device.home_events.find("event-1").home_event_voice.wav
    end

    test "an invalid wav or unknown event does not send mail" do
      post event_path, params: event_params, as: :json
      post voice_path, params: { file: Rack::Test::UploadedFile.new(StringIO.new("not wav"), "audio/wav", true, original_filename: "voice.wav") }
      assert_response :unprocessable_entity
      assert_nil @device.home_events.find("event-1").home_event_voice
      assert_empty ActionMailer::Base.deliveries

      post "/api/devices/#{@device.id}/events/missing/voice", params: { file: uploaded_wav }
      assert_response :not_found
      assert_empty ActionMailer::Base.deliveries
    end

    test "ten second raw and multipart replies are accepted and longer replies are rejected" do
      post event_path, params: event_params, as: :json
      ten_seconds = wav_bytes(160000)
      post voice_path, params: ten_seconds, headers: { "CONTENT_TYPE" => "audio/wav" }
      assert_response :created
      assert_equal ten_seconds, @device.home_events.find("event-1").home_event_voice.wav
      assert_equal ten_seconds, ActionMailer::Base.deliveries.first.attachments["voice.wav"].decoded

      upload = Rack::Test::UploadedFile.new(StringIO.new(ten_seconds), "audio/wav", true, original_filename: "voice.wav")
      post voice_path, params: { file: upload }
      assert_response :ok
      assert_equal 2, ActionMailer::Base.deliveries.size

      post voice_path, params: wav_bytes(160001), headers: { "CONTENT_TYPE" => "audio/wav" }
      assert_response :unprocessable_entity
      assert_equal ten_seconds, @device.home_events.find("event-1").home_event_voice.wav
      assert_equal 2, ActionMailer::Base.deliveries.size
    end

    test "a failed delivery stays unsent so the same upload can mail on retry" do
      post event_path, params: event_params, as: :json
      with_delivery_result(false) do
        post voice_path, params: { file: uploaded_wav }
        assert_response :created
      end
      assert_nil @device.home_events.find("event-1").messengers_notified_at
      assert_empty ActionMailer::Base.deliveries

      post voice_path, params: { file: uploaded_wav }
      assert_response :ok
      assert_equal 2, ActionMailer::Base.deliveries.size
    end

    test "a voice uploaded after a skipped notice does not send a second mail" do
      post event_path, params: event_params.merge(voice: "skipped"), as: :json
      ActionMailer::Base.deliveries.clear

      post voice_path, params: { file: uploaded_wav }
      assert_response :created
      assert_equal wav_bytes, @device.home_events.find("event-1").home_event_voice.wav
      assert_empty ActionMailer::Base.deliveries
    end

    private

    def with_delivery_result(result)
      singleton = class << HomeEventNotice; self; end
      original = singleton.instance_method(:deliver!)
      singleton.define_method(:deliver!) { |*, **| result }
      yield
    ensure
      singleton.define_method(:deliver!, original)
    end

    def event_path
      "/api/devices/#{@device.id}/events"
    end

    def voice_path
      "/api/devices/#{@device.id}/events/event-1/voice"
    end

    def event_params(type = "left_home")
      { event_id: "event-1", event_type: type, occurred_at: "2026-10-17T08:00:00Z" }
    end

    def uploaded_wav
      Rack::Test::UploadedFile.new(StringIO.new(wav_bytes), "audio/wav", true, original_filename: "voice.wav")
    end

    def wav_bytes(sample_count = 1600)
      samples = "\x00\x00".b * sample_count
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
