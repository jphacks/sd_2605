module Api
  class SessionsController < ApplicationController
    DEMO_DEVICE_ID = "entrance-1"
    DEFAULT_DEMO_PASSWORD = "tadaima-demo"

    def create
      unless Rails.env.development? || Rails.env.test?
        return render json: { error: { code: "not_found", message: "Not found." } }, status: :not_found
      end

      supplied_device_id = params[:device_id].to_s
      supplied_password = params[:password].to_s
      valid_device = secure_match?(supplied_device_id, DEMO_DEVICE_ID)
      valid_password = secure_match?(supplied_password, DEFAULT_DEMO_PASSWORD)

      unless valid_device && valid_password
        return render json: {
          error: { code: "invalid_credentials", message: "デバイスIDまたはパスワードが正しくありません。" }
        }, status: :unauthorized
      end

      render json: {
        token: SecureRandom.hex(32),
        device_id: DEMO_DEVICE_ID,
        expires_at: 8.hours.from_now.iso8601
      }
    end

    private

    def secure_match?(actual, expected)
      actual_digest = Digest::SHA256.hexdigest(actual)
      expected_digest = Digest::SHA256.hexdigest(expected)
      ActiveSupport::SecurityUtils.secure_compare(actual_digest, expected_digest)
    end
  end
end
