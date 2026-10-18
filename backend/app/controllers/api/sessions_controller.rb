module Api
  class SessionsController < ApplicationController
    DEMO_DEVICE_ID = "entrance-1"
    DEFAULT_DEMO_PASSWORD = "tadaima-demo"

    include UserAuthenticatable

    def create
      if request.headers["Authorization"].present?
        return create_user_device_session
      end

      unless Rails.env.development? || Rails.env.test?
        return render json: { error: { code: "not_found", message: "Not found." } }, status: :not_found
      end

      supplied_device_id = params[:device_id].to_s
      supplied_password = params[:password].to_s
      demo_device = Device.find_by(id: DEMO_DEVICE_ID)
      valid_device = secure_match?(supplied_device_id, DEMO_DEVICE_ID)
      valid_password = secure_match?(supplied_password, DEFAULT_DEMO_PASSWORD)

      unless valid_device && valid_password && (demo_device.nil? || demo_device.authenticate(supplied_password))
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

    def create_user_device_session
      user = user_from_token
      device = Device.find_by(id: params[:device_id].to_s.strip)
      unless user && device&.authenticate(params[:password].to_s)
        return render json: {
          error: { code: "invalid_credentials", message: "デバイスIDまたはパスワードが正しくありません。" }
        }, status: :unauthorized
      end

      device.grant_access_to!(user)

      token, expires_at = JsonWebToken.encode(user.id)
      render json: {
        token: token,
        device_id: device.id,
        expires_at: expires_at.iso8601,
        user: { id: user.id, email: user.email }
      }
    end

    def secure_match?(actual, expected)
      actual_digest = Digest::SHA256.hexdigest(actual)
      expected_digest = Digest::SHA256.hexdigest(expected)
      ActiveSupport::SecurityUtils.secure_compare(actual_digest, expected_digest)
    end
  end
end
