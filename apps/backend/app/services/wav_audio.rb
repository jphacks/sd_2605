# Matches CoreS3's RIFF parser, including trailing chunks and odd-byte padding.
class WavAudio
  MAX_BYTES = 200 * 1024

  def self.valid?(bytes)
    return false unless bytes && bytes.bytesize.between?(44, MAX_BYTES)
    return false unless bytes[0, 4] == "RIFF" && bytes[8, 4] == "WAVE"
    return false unless bytes[4, 4].unpack1("V") + 8 == bytes.bytesize

    pos = 12
    fmt = data = false
    while pos < bytes.bytesize
      return false if bytes.bytesize - pos < 8
      kind = bytes[pos, 4]
      length = bytes[pos + 4, 4].unpack1("V")
      pos += 8
      next_pos = pos + length + (length & 1)
      return false if next_pos > bytes.bytesize

      case kind
      when "fmt "
        return false if fmt || length < 16
        return false unless bytes[pos, 16].unpack("vvVVvv") == [ 1, 1, 16000, 32000, 2, 16 ]
        fmt = true
      when "data"
        return false unless fmt && !data && length.positive? && length.even? && length <= 160000
        data = true
      end
      pos = next_pos
    end
    fmt && data
  end
end
