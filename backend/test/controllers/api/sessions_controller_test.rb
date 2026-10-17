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
  end
end
