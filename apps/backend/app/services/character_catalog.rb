# Part IDs shared with face-extractor/catalog.mjs. Keep the two lists the same.
module CharacterCatalog
  CATALOG = {
    "bg" => %w[sky pink yellow mint lavender peach white gray],
    "hairColor" => %w[black darkbrown brown blonde red gray white blue pink],
    "hair" => %w[none straight-bangs curly],
    "eyeColor" => %w[black brown hazel green blue gray],
    "eyes" => %w[male female halfmoon lashed],
    "glasses" => %w[none round square cat-eye],
    "beard" => %w[none mustache full-beard],
    "mouth" => %w[tongue teeth-smile closed-smile w],
    "extras" => %w[clip ribbon freckles]
  }.freeze

  DEFAULT = {
    "bg" => "sky",
    "hairColor" => "black",
    "hair" => "none",
    "eyeColor" => "black",
    "eyes" => "male",
    "glasses" => "none",
    "beard" => "none",
    "mouth" => "closed-smile",
    "extras" => []
  }.freeze

  # Gemini may invent a value. Replace those with the default and record the field.
  def self.sanitize(raw)
    raw = raw.is_a?(Hash) ? raw.stringify_keys : {}
    out = {}
    bad = []
    CATALOG.each do |key, allowed|
      if key == "extras"
        values = raw["extras"]
        out["extras"] = values.is_a?(Array) ? values.map(&:to_s).select { |value| allowed.include?(value) } : []
      elsif allowed.include?(raw[key])
        out[key] = raw[key]
      else
        out[key] = DEFAULT[key]
        bad << key
      end
    end
    names = CATALOG.keys
    reported = raw["uncertain"].is_a?(Array) ? raw["uncertain"].map(&:to_s).select { |name| names.include?(name) } : []
    out["uncertain"] = (reported + bad).uniq
    out
  end

  # A saved face must already be inside the catalog. Returns nil when it is not.
  def self.accept(raw)
    return nil unless raw.is_a?(Hash)

    raw = raw.stringify_keys
    out = {}
    CATALOG.each do |key, allowed|
      if key == "extras"
        values = raw["extras"]
        return nil unless values.is_a?(Array)

        values = values.map(&:to_s)
        return nil unless values.uniq.length == values.length && values.all? { |value| allowed.include?(value) }

        out["extras"] = values
      else
        value = raw[key]
        return nil unless value.is_a?(String) && allowed.include?(value)

        out[key] = value
      end
    end

    uncertain = raw.fetch("uncertain", [])
    return nil unless uncertain.is_a?(Array)

    uncertain = uncertain.map(&:to_s)
    return nil unless uncertain.uniq.length == uncertain.length && uncertain.all? { |name| CATALOG.key?(name) }

    out["uncertain"] = uncertain
    out
  end
end
