require "test_helper"

module Api
  class SessionsControllerTest < ActionDispatch::IntegrationTest
    test "creates a demo session with the development credentials" do
      post "/api/sessions", params: { device_id: "entrance-1", password: "tadaima-demo" }, as: :json

      assert_response :success
      response_data = response.parsed_body
      assert_equal "entrance-1", response_data["device_id"]
      assert response_data["token"].present?
      assert response_data["expires_at"].present?
    end

    test "rejects invalid demo credentials" do
      post "/api/sessions", params: { device_id: "entrance-1", password: "wrong" }, as: :json

      assert_response :unauthorized
      assert_equal "invalid_credentials", response.parsed_body.dig("error", "code")
    end

    test "authenticates a registered device owned by the signed-in user" do
      user = User.create!(email: "owner@example.com", password: "password1")
      device = Device.create!(id: "entrance-2", user: user, password: "device-pass-1")
      user_token = JsonWebToken.encode(user.id).first

      post "/api/sessions",
        params: { device_id: device.id, password: "device-pass-1" },
        as: :json,
        headers: { "Authorization" => "Bearer #{user_token}" }

      assert_response :success
      response_data = response.parsed_body
      assert_equal device.id, response_data["device_id"]
      assert_equal user.id, JsonWebToken.decode(response_data["token"])["sub"]
    end

    test "adds another signed-in user who knows the device password" do
      owner = User.create!(email: "owner@example.com", password: "password1")
      user = User.create!(email: "other@example.com", password: "password1")
      device = Device.create!(id: "entrance-2", user: owner, password: "device-pass-1")
      user_token = JsonWebToken.encode(user.id).first

      post "/api/sessions",
        params: { device_id: device.id, password: "device-pass-1" },
        as: :json,
        headers: { "Authorization" => "Bearer #{user_token}" }

      assert_response :success
      response_data = response.parsed_body
      assert_equal device.id, response_data["device_id"]
      assert_equal user.id, JsonWebToken.decode(response_data["token"])["sub"]
      assert_equal owner, device.reload.user
      assert_includes device.members, user
    end

    test "does not add another user when the device password is wrong" do
      owner = User.create!(email: "owner@example.com", password: "password1")
      user = User.create!(email: "other@example.com", password: "password1")
      device = Device.create!(id: "entrance-2", user: owner, password: "device-pass-1")
      user_token = JsonWebToken.encode(user.id).first

      post "/api/sessions",
        params: { device_id: device.id, password: "wrong-password" },
        as: :json,
        headers: { "Authorization" => "Bearer #{user_token}" }

      assert_response :unauthorized
      assert_equal "invalid_credentials", response.parsed_body.dig("error", "code")
      assert_not_includes device.reload.members, user
    end
  end
end
