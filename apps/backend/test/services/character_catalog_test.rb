require "test_helper"

class CharacterCatalogTest < ActiveSupport::TestCase
  test "sanitize replaces an unknown part with the default and marks it uncertain" do
    face = CharacterCatalog.sanitize("glasses" => "monocle", "eyes" => "lashed", "uncertain" => [ "eyeColor", "mood" ])

    assert_equal "none", face["glasses"]
    assert_equal "lashed", face["eyes"]
    assert_equal "closed-smile", face["mouth"]
    assert_includes face["uncertain"], "glasses"
    assert_includes face["uncertain"], "eyeColor"
    assert_not_includes face["uncertain"], "mood"
  end

  test "accept keeps a complete catalog face and rejects anything outside it" do
    face = CharacterCatalog::DEFAULT.merge("glasses" => "round", "extras" => [ "clip" ], "uncertain" => [ "hair" ])
    assert_equal face, CharacterCatalog.accept(face)
    assert_nil CharacterCatalog.accept(face.merge("mouth" => "grin"))
    assert_nil CharacterCatalog.accept(face.merge("extras" => [ "clip", "clip" ]))
    assert_nil CharacterCatalog.accept(face.except("bg"))
  end
end
