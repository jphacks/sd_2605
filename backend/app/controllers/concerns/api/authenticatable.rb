module Api
  module Authenticatable
    extend ActiveSupport::Concern

    private

    def render_auth_token(user, status: :ok)
      token, expires_at = JsonWebToken.encode(user.id)
      render json: {
        token: token,
        expires_at: expires_at.iso8601,
        user: { id: user.id, email: user.email }
      }, status: status
    end

    def render_invalid_params(user)
      render json: {
        error: { code: "invalid_params", message: invalid_params_message(user) }
      }, status: :unprocessable_entity
    end

    def invalid_params_message(user)
      if user.errors.of_kind?(:email, :taken)
        "このメールアドレスはすでに登録されています。"
      elsif user.errors.of_kind?(:email, :blank)
        "メールアドレスを入力してください。"
      elsif user.errors.where(:email).any?
        "メールアドレスの形式が正しくありません。"
      elsif user.errors.of_kind?(:password, :blank)
        "パスワードを入力してください。"
      elsif user.errors.where(:password).any?
        "パスワードは8文字以上にしてください。"
      else
        "入力内容を確認してください。"
      end
    end
  end
end
