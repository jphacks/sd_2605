require "test_helper"
require "fileutils"

class PushWorkerControllerTest < ActionDispatch::IntegrationTest
  setup do
    @path = Rails.root.join("spa", "push-worker.js")
    @original = @path.binread if @path.file?
    FileUtils.mkdir_p(@path.dirname)
    File.binwrite(@path, "self.addEventListener('push', () => {});\n")
  end

  teardown do
    if @original
      File.binwrite(@path, @original)
    elsif @path.file?
      File.delete(@path)
    end
  end

  test "worker is JavaScript at the root scope with caching disabled" do
    get "/push-worker.js"
    assert_response :success
    assert_equal "application/javascript", response.media_type
    assert_equal "/", response.headers["Service-Worker-Allowed"]
    assert_includes response.headers["Cache-Control"], "no-store"
    assert_includes response.body, "addEventListener"
  end
end
