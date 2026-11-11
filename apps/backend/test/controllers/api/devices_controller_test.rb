require "test_helper"

module Api
  class DevicesControllerTest < ActionDispatch::IntegrationTest
    setup do
      @user = User.create!(email: "owner@example.com", password: "password1")
      @token = JsonWebToken.encode(@user.id).first
      @headers = { "Authorization" => "Bearer #{@token}" }
    end

    test "lists the signed-in user's homes with their roles in device id order" do
      other_owner = User.create!(email: "other-owner@example.com", password: "password1")
      home = Device.create!(id: "home-b", user: @user, password: "device-pass-1")
      Device.create!(id: "home-a", user: other_owner, password: "device-pass-1").grant_access_to!(@user, role: "messenger")
      Device.create!(id: "other-home", user: other_owner, password: "device-pass-1")

      get "/api/devices", headers: @headers

      assert_response :success
      assert_equal [
        { "device_id" => "home-a", "role" => "messenger" },
        { "device_id" => "home-b", "role" => "resident" }
      ], response.parsed_body["devices"]
      assert_not_includes response.body, "device-pass-1"
      assert_not_includes response.body, home.password_digest
    end

    test "returns an empty device list when the user has joined no homes" do
      get "/api/devices", headers: @headers

      assert_response :success
      assert_equal({ "devices" => [] }, response.parsed_body)
    end

    test "rejects the device list without a user session" do
      get "/api/devices"

      assert_response :unauthorized
      assert_equal "invalid_credentials", response.parsed_body.dig("error", "code")
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
      assert_equal "resident", response.parsed_body["role"]
      device = Device.find("entrance-2")
      assert_equal @user, device.user
      assert_equal "resident", device.device_memberships.find_by!(user: @user).role
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
      assert_equal "resident", response.parsed_body["role"]
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
      assert_equal "resident", device.device_memberships.find_by!(user: @user).role
      assert_equal "resident", device.device_memberships.find_by!(user: other_user).role
      assert_equal "resident", response.parsed_body["role"]
    end

    test "returns the existing messenger role when that user adds the device with its password" do
      device = Device.create!(id: "entrance-2", user: @user, password: "device-pass-1")
      messenger = User.create!(email: "messenger@example.com", password: "password1")
      device.grant_access_to!(messenger, role: "messenger")

      post "/api/devices",
        params: { device_id: "entrance-2", password: "device-pass-1" },
        as: :json,
        headers: { "Authorization" => "Bearer #{JsonWebToken.encode(messenger.id).first}" }

      assert_response :success
      assert_equal "messenger", response.parsed_body["role"]
      assert_equal "messenger", device.device_memberships.find_by!(user: messenger).role
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
