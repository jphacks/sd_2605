module Api
  class UserSessionsController < ApplicationController
    include Authenticatable

    def create
      user = User.find_by(email: normalized_email)
      unless user&.authenticate(password)
        return render json: {
          error: {
            code: "invalid_credentials",
            message: "メールアドレスまたはパスワードが正しくありません。"
          }
        }, status: :unauthorized
      end

      render_auth_token(user)
    end

    private

    def normalized_email
      params[:email].to_s.strip.downcase
    end

    def password
      params[:password].to_s
    end
  end
end
