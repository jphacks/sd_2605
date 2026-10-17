require "test_helper"

class WavAudioTest < ActiveSupport::TestCase
  def wav(samples = 16000)
    data = "\0\0".b * samples
    "RIFF".b + [ 36 + data.bytesize ].pack("V") + "WAVEfmt ".b +
      [ 16, 1, 1, 16000, 32000, 2, 16 ].pack("VvvVVvv") + "data".b + [ data.bytesize ].pack("V") + data
  end

  test "default voices and exact five second PCM are accepted" do
    %w[leave arrive].each do |slot|
      assert WavAudio.valid?(File.binread(Rails.root.join("config", "audio", "#{slot}.wav")))
    end
    assert WavAudio.valid?(wav(80000))
    assert_not WavAudio.valid?(wav(80001))
  end

  test "rejects truncated RIFF and incorrect PCM parameters" do
    assert_not WavAudio.valid?(wav[0...-1])
    [ [ 20, [ 3 ].pack("v") ], [ 22, [ 2 ].pack("v") ], [ 24, [ 44100 ].pack("V") ],
      [ 28, [ 1 ].pack("V") ], [ 32, [ 1 ].pack("v") ], [ 34, [ 8 ].pack("v") ] ].each do |pos, value|
      bytes = wav
      bytes[pos, value.bytesize] = value
      assert_not WavAudio.valid?(bytes), "accepted invalid PCM field at #{pos}"
    end
  end

  test "walks trailing chunks, padding, and rejects oversized chunks or garbage" do
    bytes = wav + "JUNK" + [ 1 ].pack("V") + "x\0"
    bytes[4, 4] = [ bytes.bytesize - 8 ].pack("V")
    assert WavAudio.valid?(bytes)
    bytes[-6, 4] = [ 0xffffffff ].pack("V")
    assert_not WavAudio.valid?(bytes)
    assert_not WavAudio.valid?(wav + "garbage")
    assert_not WavAudio.valid?("x" * (WavAudio::MAX_BYTES + 1))
    assert_not WavAudio.valid?(nil)
  end
end
