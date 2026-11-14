require "test_helper"
require "json"
require "base64"

class CharacterExtractorTest < ActiveSupport::TestCase
  FakeResponse = Struct.new(:code, :body)
  FakeHttp = Struct.new(:response) do
    attr_accessor :sent

    def new(_host, _port)
      self
    end

    def use_ssl=(_value); end
    def open_timeout=(_value); end
    def read_timeout=(_value); end

    def request(req)
      self.sent = req
      response
    end
  end

  setup do
    @previous_key = ENV["GEMINI_API_KEY"]
    @previous_model = ENV["GEMINI_MODEL"]
    ENV["GEMINI_API_KEY"] = "test-key"
    ENV["GEMINI_MODEL"] = "gemini-test"
  end

  teardown do
    if @previous_key
      ENV["GEMINI_API_KEY"] = @previous_key
    else
      ENV.delete("GEMINI_API_KEY")
    end
    if @previous_model
      ENV["GEMINI_MODEL"] = @previous_model
    else
      ENV.delete("GEMINI_MODEL")
    end
  end

  test "sends the photo once and returns only catalog fields" do
    body = {
      "candidates" => [ {
        "content" => { "parts" => [ { "text" => { "glasses" => "round", "eyes" => "lashed", "beard" => "cape" }.to_json } ] }
      } ]
    }
    http = FakeHttp.new(FakeResponse.new("200", body.to_json))

    face = CharacterExtractor.call("photo-bytes", "image/jpeg", http: http)

    assert_equal "round", face["glasses"]
    assert_equal "lashed", face["eyes"]
    assert_equal "none", face["beard"]
    assert_includes face["uncertain"], "beard"
    payload = JSON.parse(http.sent.body)
    assert_equal "photo-bytes", Base64.strict_decode64(payload.dig("contents", 0, "parts", 0, "inlineData", "data"))
    assert_equal "image/jpeg", payload.dig("contents", 0, "parts", 0, "inlineData", "mimeType")
    assert_equal "test-key", http.sent["x-goog-api-key"]
    assert_includes http.sent.path, "/models/gemini-test:generateContent"
  end

  test "a busy upstream is an error and not a default face" do
    http = FakeHttp.new(FakeResponse.new("503", "{}"))

    error = assert_raises(CharacterExtractor::Error) do
      CharacterExtractor.call("photo-bytes", "image/png", http: http)
    end
    assert_equal "upstream 503", error.message
  end

  test "a missing key fails before any request" do
    ENV.delete("GEMINI_API_KEY")
    http = FakeHttp.new(FakeResponse.new("200", "{}"))

    assert_raises(CharacterExtractor::Error) do
      CharacterExtractor.call("photo-bytes", "image/png", http: http)
    end
    assert_nil http.sent
  end

  test "an oversized photo is rejected before any request" do
    http = FakeHttp.new(FakeResponse.new("200", "{}"))

    assert_raises(CharacterExtractor::InvalidImage) do
      CharacterExtractor.call("x" * (CharacterExtractor::MAX_BYTES + 1), "image/webp", http: http)
    end
    assert_nil http.sent
  end

  test "a model name cannot leave the Gemini model path" do
    ENV["GEMINI_MODEL"] = "../evil"
    http = FakeHttp.new(FakeResponse.new("200", "{}"))

    error = assert_raises(CharacterExtractor::Error) do
      CharacterExtractor.call("photo-bytes", "image/png", http: http)
    end
    assert_equal "invalid model", error.message
    assert_nil http.sent
  end

  test "a network failure is an error and not a default face" do
    http = FakeHttp.new(FakeResponse.new("200", "{}"))
    def http.request(_req)
      raise SocketError, "down"
    end

    error = assert_raises(CharacterExtractor::Error) do
      CharacterExtractor.call("photo-bytes", "image/png", http: http)
    end
    assert_equal "SocketError", error.message
  end
end
