require "test_helper"

module Api
  class GoogleSessionsControllerTest < ActionDispatch::IntegrationTest
    CLIENT_ID = "test-google-client"
    TOKEN = "good-token"

    setup do
      @previous_client_id = ENV["GOOGLE_CLIENT_ID"]
      ENV["GOOGLE_CLIENT_ID"] = CLIENT_ID
    end

    teardown do
      ENV["GOOGLE_CLIENT_ID"] = @previous_client_id
    end

    test "creates a user from a verified Google identity and returns a token" do
      assert_difference -> { User.count }, 1 do
        post_google(payload)
      end

      assert_response :success
      user = User.find_by!(email: "person@example.com")
      assert_equal "google-sub-1", user.google_sub
      assert_nil user.password_digest
      body = response.parsed_body
      assert_equal user.id, body.dig("user", "id")
      assert_equal "person@example.com", body.dig("user", "email")
      assert_equal user.id, JsonWebToken.decode(body["token"])["sub"]
      assert_not_includes response.body, TOKEN
    end

    test "signs in the same Google account without creating another user" do
      User.create!(email: "person@example.com", google_sub: "google-sub-1")

      assert_no_difference -> { User.count } do
        post_google(payload)
      end

      assert_response :success
      assert_equal "person@example.com", response.parsed_body.dig("user", "email")
    end

    test "links a verified Google account to the existing password account" do
      user = User.create!(email: "person@example.com", password: "password1")

      assert_no_difference -> { User.count } do
        post_google(payload)
      end

      assert_response :success
      assert_equal user.id, response.parsed_body.dig("user", "id")
      assert_equal "google-sub-1", user.reload.google_sub
      assert user.authenticate("password1")
    end

    test "does not replace a Google account already linked to the email" do
      user = User.create!(email: "person@example.com", password: "password1", google_sub: "other-sub")

      assert_no_difference -> { User.count } do
        post_google(payload)
      end

      assert_response :unauthorized
      assert_equal "invalid_google_token", response.parsed_body.dig("error", "code")
      assert_equal "other-sub", user.reload.google_sub
    end

    test "rejects an unverified email without creating a user" do
      assert_no_difference -> { User.count } do
        post_google(payload.merge("email_verified" => false))
      end

      assert_response :unauthorized
      assert_equal "Googleログインに失敗しました。もう一度お試しください。", response.parsed_body.dig("error", "message")
    end

    test "rejects an invalid Google token" do
      with_google_verification(-> { raise Google::Auth::IDTokens::VerificationError, "bad" }) do
        assert_no_difference -> { User.count } do
          post "/api/google_sessions", params: { credential: TOKEN }, as: :json
        end
      end

      assert_response :unauthorized
      assert_equal "invalid_google_token", response.parsed_body.dig("error", "code")
    end

    test "rejects Google sign-in when the client id is not configured" do
      ENV["GOOGLE_CLIENT_ID"] = nil
      called = false
      with_google_verification(-> { called = true }) do
        post "/api/google_sessions", params: { credential: TOKEN }, as: :json
      end

      assert_not called
      assert_response :service_unavailable
      assert_equal "google_unavailable", response.parsed_body.dig("error", "code")
    end

    test "returns the configured Google client id" do
      get "/api/auth_config"

      assert_response :success
      assert_equal CLIENT_ID, response.parsed_body["google_client_id"]
    end

    test "returns a null client id when Google sign-in is not configured" do
      ENV["GOOGLE_CLIENT_ID"] = " "
      get "/api/auth_config"

      assert_response :success
      assert_nil response.parsed_body["google_client_id"]
    end

    private

    def payload
      { "sub" => "google-sub-1", "email" => " Person@Example.com ", "email_verified" => true }
    end

    def post_google(identity)
      seen = with_google_verification(-> { identity }) do
        post "/api/google_sessions", params: { credential: TOKEN }, as: :json
      end
      assert_equal [ [ TOKEN, CLIENT_ID ] ], seen
    end

    def with_google_verification(result)
      seen = []
      singleton = Google::Auth::IDTokens.singleton_class
      original = Google::Auth::IDTokens.method(:verify_oidc)
      singleton.define_method(:verify_oidc) do |token, aud: nil, **|
        seen << [ token, aud ]
        result.call
      end
      yield
      seen
    ensure
      singleton.define_method(:verify_oidc, original) if original
    end
  end
end
