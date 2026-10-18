module Api
  class AuthConfigController < ApplicationController
    def show
      render json: { google_client_id: ENV["GOOGLE_CLIENT_ID"].presence }
    end
  end
end
