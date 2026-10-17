module Api
  module UserAuthenticatable
    extend ActiveSupport::Concern

    private

    attr_reader :current_user

    def authenticate_user!
      @current_user = user_from_token
      return if current_user

      render_unauthorized
    end

    def user_from_token
      token = request.headers["Authorization"].to_s.match(/\ABearer\s+(.+)\z/i)&.captures&.first
      payload = JsonWebToken.decode(token) if token.present?
      User.find_by(id: payload["sub"]) if payload
    rescue JWT::DecodeError
      nil
    end

    def render_unauthorized
      render json: {
        error: { code: "invalid_credentials", message: "ログインしてください。" }
      }, status: :unauthorized
    end
  end
end
