module Api
  class UsersController < ApplicationController
    include Authenticatable

    def create
      user = User.new(user_params)
      user.save!
      render_auth_token(user, status: :created)
    rescue ActiveRecord::RecordInvalid
      render_invalid_params(user)
    rescue ActiveRecord::RecordNotUnique
      user.errors.add(:email, :taken)
      render_invalid_params(user)
    end

    private

    def user_params
      params.permit(:email, :password)
    end
  end
end
