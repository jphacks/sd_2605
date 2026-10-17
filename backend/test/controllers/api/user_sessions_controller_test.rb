require "test_helper"

module Api
  class UserSessionsControllerTest < ActionDispatch::IntegrationTest
    setup do
      @user = User.create!(email: "user@example.com", password: "password1")
    end

    test "logs in with email and password" do
      post "/api/user_sessions",
        params: { email: " User@Example.com ", password: "password1" },
        as: :json

      assert_response :success
      body = response.parsed_body
      assert_equal @user.id, body.dig("user", "id")
      assert_equal "user@example.com", body.dig("user", "email")
      payload = JsonWebToken.decode(body["token"])
      assert_equal @user.id, payload["sub"]
      assert_in_delta 7.days.from_now, Time.iso8601(body["expires_at"]), 5
    end

    test "rejects a wrong password" do
      post "/api/user_sessions",
        params: { email: "user@example.com", password: "wrong-password" },
        as: :json

      assert_response :unauthorized
      assert_equal "invalid_credentials", response.parsed_body.dig("error", "code")
      assert_equal "メールアドレスまたはパスワードが正しくありません。", response.parsed_body.dig("error", "message")
    end

    test "rejects an unknown email with the same error" do
      post "/api/user_sessions",
        params: { email: "missing@example.com", password: "password1" },
        as: :json

      assert_response :unauthorized
      assert_equal "invalid_credentials", response.parsed_body.dig("error", "code")
      assert_equal "メールアドレスまたはパスワードが正しくありません。", response.parsed_body.dig("error", "message")
    end
  end
end
