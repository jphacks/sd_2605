module Api
  class GoogleSessionsController < ApplicationController
    include Authenticatable

    def create
      client_id = ENV["GOOGLE_CLIENT_ID"].presence
      return render_google_unavailable unless client_id

      payload = Google::Auth::IDTokens.verify_oidc(params[:credential].to_s, aud: client_id)
      user = User.from_google_identity(payload)
      return render_invalid_google unless user

      render_auth_token(user)
    rescue Google::Auth::IDTokens::KeySourceError
      render_google_unavailable
    rescue Google::Auth::IDTokens::VerificationError
      render_invalid_google
    end

    private

    def render_google_unavailable
      render json: {
        error: { code: "google_unavailable", message: "Googleログインは現在利用できません。" }
      }, status: :service_unavailable
    end

    def render_invalid_google
      render json: {
        error: { code: "invalid_google_token", message: "Googleログインに失敗しました。もう一度お試しください。" }
      }, status: :unauthorized
    end
  end
end
