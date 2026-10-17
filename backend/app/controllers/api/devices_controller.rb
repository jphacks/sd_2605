module Api
  class DevicesController < ApplicationController
    include UserAuthenticatable

    before_action :authenticate_user!

    def create
      device = current_user.devices.find_by(id: device_id)
      if device
        unless device.authenticate(device_password)
          return render_device_conflict
        end

        return render json: { device_id: device.id }
      end

      device = current_user.devices.build(id: device_id, password: device_password)
      device.save!
      render json: { device_id: device.id }, status: :created
    rescue ActiveRecord::RecordInvalid => e
      render json: {
        error: { code: "invalid_params", message: device_error_message(e.record) }
      }, status: :unprocessable_entity
    rescue ActiveRecord::RecordNotUnique
      render_device_conflict
    end

    private

    def device_id
      params[:device_id].to_s.strip
    end

    def device_password
      params[:password].to_s
    end

    def render_device_conflict
      render json: {
        error: { code: "device_id_taken", message: "このデバイスIDは使用できません。IDとパスワードを確認してください。" }
      }, status: :conflict
    end

    def device_error_message(device)
      if device.errors.where(:id).any?
        "デバイスIDは英数字・ハイフン・アンダースコアの1〜64文字で入力してください。"
      elsif device.errors.of_kind?(:password, :blank)
        "パスワードを入力してください。"
      elsif device.errors.where(:password).any?
        "パスワードは8文字以上にしてください。"
      else
        "入力内容を確認してください。"
      end
    end
  end
end
