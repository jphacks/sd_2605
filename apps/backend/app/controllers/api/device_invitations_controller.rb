module Api
  class DeviceInvitationsController < ApplicationController
    include UserAuthenticatable

    before_action :authenticate_user!

    def create
      device = Device.find_by(id: params[:device_id])
      return render_error(:not_found, "not_found", "Unknown device") unless device
      unless device.resident?(current_user)
        return render_error(:forbidden, "resident_only", "招待を発行できるのは、この家の居住者だけです。")
      end

      invitation = DeviceInvitation.issue!(device: device, invited_by: current_user)
      render json: { code: invitation.code, expires_at: invitation.expires_at.iso8601 }, status: :created
    end

    private

    def render_error(status, code, message)
      render json: { error: { code: code, message: message } }, status: status
    end
  end
end
