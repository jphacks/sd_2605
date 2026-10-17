require "test_helper"

module Api
  class UsersControllerTest < ActionDispatch::IntegrationTest
    test "registers a user and returns a token" do
      assert_difference -> { User.count }, 1 do
        post "/api/users", params: { email: " User@Example.com ", password: "password1" }, as: :json
      end

      assert_response :created
      body = response.parsed_body
      user = User.find_by!(email: "user@example.com")
      assert_equal user.id, body.dig("user", "id")
      assert_equal "user@example.com", body.dig("user", "email")
      assert body["token"].present?
      assert_not_includes response.body, "password1"
      assert_not_includes response.body, user.password_digest

      payload = JsonWebToken.decode(body["token"])
      assert_equal user.id, payload["sub"]
      assert_in_delta 7.days.from_now, Time.iso8601(body["expires_at"]), 5
    end

    test "rejects a duplicate email regardless of case" do
      User.create!(email: "user@example.com", password: "password1")

      post "/api/users", params: { email: "USER@example.com", password: "password2" }, as: :json

      assert_response :unprocessable_entity
      assert_equal "invalid_params", response.parsed_body.dig("error", "code")
      assert_equal "このメールアドレスはすでに登録されています。", response.parsed_body.dig("error", "message")
    end

    test "rejects an invalid email" do
      post "/api/users", params: { email: "not-an-email", password: "password1" }, as: :json

      assert_response :unprocessable_entity
      assert_equal "invalid_params", response.parsed_body.dig("error", "code")
      assert_equal "メールアドレスの形式が正しくありません。", response.parsed_body.dig("error", "message")
    end

    test "rejects a short password" do
      post "/api/users", params: { email: "user@example.com", password: "short" }, as: :json

      assert_response :unprocessable_entity
      assert_equal "invalid_params", response.parsed_body.dig("error", "code")
      assert_equal "パスワードは8文字以上にしてください。", response.parsed_body.dig("error", "message")
    end
  end
end
