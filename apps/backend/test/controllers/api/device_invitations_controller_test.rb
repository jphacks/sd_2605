require "test_helper"

module Api
  class DeviceInvitationsControllerTest < ActionDispatch::IntegrationTest
    setup do
      @resident = User.create!(email: "invite-resident@example.com", password: "password1")
      @device = Device.create!(id: "invite-device", user: @resident, password: "device-pass-1")
      @headers = { "Authorization" => "Bearer #{JsonWebToken.encode(@resident.id).first}" }
    end

    test "a resident receives a single-use code" do
      post "/api/devices/#{@device.id}/invitations", headers: @headers

      assert_response :created
      code = response.parsed_body["code"]
      assert_match(/\A[ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{8}\z/, code)
      assert_in_delta 24.hours.from_now, Time.iso8601(response.parsed_body["expires_at"]), 5
      assert_equal code, @device.device_invitations.find_by!(code: code).code
    end

    test "an unknown device returns not found" do
      post "/api/devices/missing-device/invitations", headers: @headers

      assert_response :not_found
      assert_equal "not_found", response.parsed_body.dig("error", "code")
    end

    test "a messenger cannot issue an invitation" do
      messenger = User.create!(email: "invite-messenger@example.com", password: "password1")
      @device.grant_access_to!(messenger, role: "messenger")
      headers = { "Authorization" => "Bearer #{JsonWebToken.encode(messenger.id).first}" }

      post "/api/devices/#{@device.id}/invitations", headers: headers

      assert_response :forbidden
      assert_equal "resident_only", response.parsed_body.dig("error", "code")
    end

    test "accepting a code makes the signed-in user a messenger" do
      post "/api/devices/#{@device.id}/invitations", headers: @headers
      code = response.parsed_body["code"]
      messenger = User.create!(email: "invite-accept@example.com", password: "password1")
      headers = { "Authorization" => "Bearer #{JsonWebToken.encode(messenger.id).first}" }

      post "/api/invitations/accept", params: { code: code.downcase }, as: :json, headers: headers

      assert_response :success
      assert_equal @device.id, response.parsed_body["device_id"]
      assert_equal "messenger", response.parsed_body["role"]
      assert @device.messenger?(messenger)
      assert_not @device.device_invitations.find_by!(code: code).acceptable?
    end

    test "a used, expired, or unknown code is rejected" do
      post "/api/devices/#{@device.id}/invitations", headers: @headers
      code = response.parsed_body["code"]
      messenger = User.create!(email: "invite-used@example.com", password: "password1")
      headers = { "Authorization" => "Bearer #{JsonWebToken.encode(messenger.id).first}" }
      post "/api/invitations/accept", params: { code: code }, as: :json, headers: headers
      assert_response :success

      other = User.create!(email: "invite-other@example.com", password: "password1")
      other_headers = { "Authorization" => "Bearer #{JsonWebToken.encode(other.id).first}" }
      post "/api/invitations/accept", params: { code: code }, as: :json, headers: other_headers
      assert_response :unprocessable_entity
      assert_equal "invalid_invitation", response.parsed_body.dig("error", "code")
      assert_not @device.messenger?(other)

      post "/api/devices/#{@device.id}/invitations", headers: @headers
      expired = response.parsed_body["code"]
      travel 25.hours do
        post "/api/invitations/accept", params: { code: expired }, as: :json, headers: other_headers
      end
      assert_response :unprocessable_entity
      assert_not @device.messenger?(other)

      post "/api/invitations/accept", params: { code: "NOPE1234" }, as: :json, headers: other_headers
      assert_response :unprocessable_entity
    end

    test "a resident of the same home is not changed into a messenger" do
      post "/api/devices/#{@device.id}/invitations", headers: @headers
      code = response.parsed_body["code"]

      post "/api/invitations/accept", params: { code: code }, as: :json, headers: @headers

      assert_response :unprocessable_entity
      assert_equal "already_resident", response.parsed_body.dig("error", "code")
      assert @device.device_invitations.find_by!(code: code).acceptable?
      assert_equal "resident", @device.device_memberships.find_by!(user: @resident).role
    end

    test "an existing messenger stays a messenger and the code is spent" do
      messenger = User.create!(email: "invite-again@example.com", password: "password1")
      @device.grant_access_to!(messenger, role: "messenger")
      post "/api/devices/#{@device.id}/invitations", headers: @headers
      code = response.parsed_body["code"]
      headers = { "Authorization" => "Bearer #{JsonWebToken.encode(messenger.id).first}" }

      post "/api/invitations/accept", params: { code: code }, as: :json, headers: headers

      assert_response :success
      assert_equal "messenger", @device.device_memberships.find_by!(user: messenger).role
      assert_not @device.device_invitations.find_by!(code: code).acceptable?
    end

    test "accepting requires a signed-in user" do
      post "/api/invitations/accept", params: { code: "ABCD2345" }, as: :json

      assert_response :unauthorized
    end
  end
end
