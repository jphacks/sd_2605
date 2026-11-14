require "net/http"
require "json"
require "base64"
require "openssl"

# One photo in, one catalog dictionary out. The photo is not stored.
class CharacterExtractor
  class Error < StandardError; end
  class InvalidImage < Error; end

  MAX_BYTES = 4 * 1024 * 1024
  MIME_TYPES = {
    "image/jpeg" => "image/jpeg",
    "image/jpg" => "image/jpeg",
    "image/png" => "image/png",
    "image/webp" => "image/webp"
  }.freeze
  DEFAULT_MODEL = "gemini-3.8-flash"
  OPEN_TIMEOUT = 5
  # nil waits until Gemini responds. The free tier can sit well past two minutes.
  READ_TIMEOUT = nil

  def self.call(bytes, mime_type, http: Net::HTTP)
    normalized = MIME_TYPES[mime_type.to_s.split(";").first.to_s.strip.downcase]
    raise InvalidImage, "unsupported image" unless normalized
    raise InvalidImage, "empty image" unless bytes.is_a?(String) && bytes.bytesize.positive?
    raise InvalidImage, "image too large" if bytes.bytesize > MAX_BYTES

    new(http: http).extract(bytes, normalized)
  end

  def initialize(http: Net::HTTP)
    @http = http
  end

  def extract(bytes, mime_type)
    key = ENV["GEMINI_API_KEY"].to_s
    raise Error, "missing API key" if key.empty?

    model = ENV["GEMINI_MODEL"].presence || DEFAULT_MODEL
    raise Error, "invalid model" unless model.match?(/\A[A-Za-z0-9._-]+\z/)
    uri = URI("https://generativelanguage.googleapis.com/v1beta/models/#{model}:generateContent")
    client = @http.new(uri.host, uri.port)
    client.use_ssl = true
    client.open_timeout = OPEN_TIMEOUT
    client.read_timeout = READ_TIMEOUT
    request = Net::HTTP::Post.new(uri)
    request["Content-Type"] = "application/json"
    request["x-goog-api-key"] = key
    request.body = {
      contents: [ {
        parts: [
          { inlineData: { mimeType: mime_type, data: Base64.strict_encode64(bytes) } },
          { text: PROMPT }
        ]
      } ],
      generationConfig: {
        temperature: 0.2,
        responseMimeType: "application/json",
        responseSchema: response_schema
      }
    }.to_json

    response = client.request(request)
    parse_response(response.code, response.body)
  rescue InvalidImage
    raise
  rescue Error
    raise
  rescue JSON::ParserError, Timeout::Error, IOError, SystemCallError, SocketError, OpenSSL::SSL::SSLError => e
    raise Error, e.class.name
  end

  def parse_response(status, body)
    raise Error, "upstream #{status}" unless status.to_i == 200

    text = JSON.parse(body.to_s).dig("candidates", 0, "content", "parts")&.filter_map { |part| part["text"] }&.join
    raise Error, "empty response" if text.blank?

    CharacterCatalog.sanitize(JSON.parse(text))
  end

  private

  def response_schema
    properties = {}
    CharacterCatalog::CATALOG.each do |key, values|
      item = { type: "STRING", enum: values }
      properties[key] = key == "extras" ? { type: "ARRAY", items: item } : item
    end
    properties["uncertain"] = {
      type: "ARRAY",
      description: "Names of fields you are not confident about",
      items: { type: "STRING", enum: CharacterCatalog::CATALOG.keys }
    }
    {
      type: "OBJECT",
      properties: properties,
      required: CharacterCatalog::CATALOG.keys + [ "uncertain" ],
      propertyOrdering: CharacterCatalog::CATALOG.keys + [ "uncertain" ]
    }
  end

  PROMPT = <<~TEXT.freeze
    You convert a photo of a person into a cartoon character made of fixed parts.
    Look at the person's face and, for each field, choose the option that best matches.
    Rules:
    - Choose only from the allowed values.
    - bg: pick a pastel that suits the person's skin tone or clothing. It is a style choice, not a skin match.
    - hair: pick "none" unless the person clearly has bangs/fringe covering the forehead.
    - eyes: choose by how the person's eyes look in the photo (shape, lashes, makeup).
      - "male": plain round eyes.
      - "female": eyes with eyeliner (a winged upper lid line).
      - "halfmoon": half-moon, relaxed or smiling eyes.
      - "lashed": round eyes with lashes but no eyeliner.
      Do not infer gender, age or ethnicity.
    - eyeColor: the iris colour. If it is hard to see, pick "black" or "brown" and add "eyeColor" to uncertain.
    - beard: "none" if clean-shaven or no clear facial hair.
    - mouth: based on the smile in the photo (teeth visible -> teeth-smile).
    - extras: only include items that are clearly visible (e.g. freckles).
    - Do not guess gender, age or ethnicity. Do not output anything except the JSON.
  TEXT
end
