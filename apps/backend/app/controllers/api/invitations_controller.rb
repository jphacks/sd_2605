module Api
  class InvitationsController < ApplicationController
    include UserAuthenticatable

    before_action :authenticate_user!

    def accept
      invitation = DeviceInvitation.find_by(code: params[:code].to_s.strip.upcase)
      return invalid_invitation unless invitation

      invitation.with_lock do
        return invalid_invitation unless invitation.acceptable?
        if invitation.device.resident?(current_user)
          return render_error(:unprocessable_entity, "already_resident", "この家の居住者は差し出し手に変更できません。")
        end

        invitation.device.grant_access_to!(current_user, role: "messenger") unless invitation.device.messenger?(current_user)
        invitation.update!(accepted_by: current_user, accepted_at: Time.current)
      end

      render json: { device_id: invitation.device_id, role: "messenger" }
    end

    private

    def invalid_invitation
      render_error(:unprocessable_entity, "invalid_invitation", "この招待コードは使えません。")
    end

    def render_error(status, code, message)
      render json: { error: { code: code, message: message } }, status: status
    end
  end
end
