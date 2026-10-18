require "test_helper"

module Api
  class DevicesControllerTest < ActionDispatch::IntegrationTest
    setup do
      @user = User.create!(email: "owner@example.com", password: "password1")
      @token = JsonWebToken.encode(@user.id).first
      @headers = { "Authorization" => "Bearer #{@token}" }
    end

    test "registers a device for the signed-in user and stores its password hashed" do
      assert_difference -> { Device.count }, 1 do
        post "/api/devices",
          params: { device_id: " entrance-2 ", password: "device-pass-1" },
          as: :json,
          headers: @headers
      end

      assert_response :created
      assert_equal "entrance-2", response.parsed_body["device_id"]
      device = Device.find("entrance-2")
      assert_equal @user, device.user
      assert device.authenticate("device-pass-1")
      assert_not_includes response.body, "device-pass-1"
      assert_not_includes response.body, device.password_digest
    end

    test "allows the owner to add an existing device again with its password" do
      Device.create!(id: "entrance-2", user: @user, password: "device-pass-1")

      assert_no_difference -> { Device.count } do
        post "/api/devices",
          params: { device_id: "entrance-2", password: "device-pass-1" },
          as: :json,
          headers: @headers
      end

      assert_response :success
      assert_equal "entrance-2", response.parsed_body["device_id"]
    end

    test "rejects registration without a user session" do
      post "/api/devices", params: { device_id: "entrance-2", password: "device-pass-1" }, as: :json

      assert_response :unauthorized
      assert_equal "invalid_credentials", response.parsed_body.dig("error", "code")
    end

    test "rejects invalid device ids and short passwords" do
      post "/api/devices",
        params: { device_id: "invalid/id", password: "short" },
        as: :json,
        headers: @headers

      assert_response :unprocessable_entity
      assert_equal "invalid_params", response.parsed_body.dig("error", "code")
    end

    test "adds another user who knows the password without changing the registrant" do
      device = Device.create!(id: "entrance-2", user: @user, password: "device-pass-1")
      other_user = User.create!(email: "other@example.com", password: "password1")
      other_token = JsonWebToken.encode(other_user.id).first

      assert_no_difference -> { Device.count } do
        post "/api/devices",
          params: { device_id: "entrance-2", password: "device-pass-1" },
          as: :json,
          headers: { "Authorization" => "Bearer #{other_token}" }
      end

      assert_response :success
      assert_equal "entrance-2", response.parsed_body["device_id"]
      assert_equal @user, device.reload.user
      assert_includes device.members, other_user
      assert_includes device.members, @user
    end

    test "does not add another user when the device password is wrong" do
      device = Device.create!(id: "entrance-2", user: @user, password: "device-pass-1")
      other_user = User.create!(email: "other@example.com", password: "password1")
      other_token = JsonWebToken.encode(other_user.id).first

      post "/api/devices",
        params: { device_id: "entrance-2", password: "wrong-password" },
        as: :json,
        headers: { "Authorization" => "Bearer #{other_token}" }

      assert_response :conflict
      assert_equal "device_id_taken", response.parsed_body.dig("error", "code")
      assert_equal @user, device.reload.user
      assert_not_includes device.members, other_user
    end
  end
end
