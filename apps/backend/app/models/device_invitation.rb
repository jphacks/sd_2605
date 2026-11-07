class DeviceInvitation < ApplicationRecord
  CODE_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
  EXPIRY = 24.hours

  belongs_to :device
  belongs_to :invited_by, class_name: "User", inverse_of: :device_invitations
  belongs_to :accepted_by, class_name: "User", optional: true, inverse_of: :accepted_device_invitations

  validates :code, presence: true, uniqueness: true, format: { with: /\A[#{CODE_ALPHABET}]{8}\z/o }
  validates :expires_at, presence: true

  def self.issue!(device:, invited_by:)
    5.times do
      invitation = new(
        device: device,
        invited_by: invited_by,
        code: generate_code,
        expires_at: EXPIRY.from_now
      )
      begin
        return invitation if invitation.save
      rescue ActiveRecord::RecordNotUnique
        next
      end
      raise ActiveRecord::RecordInvalid, invitation unless invitation.errors.of_kind?(:code, :taken)
    end
    raise ActiveRecord::RecordNotUnique, "invitation code"
  end

  def self.generate_code
    Array.new(8) { CODE_ALPHABET[SecureRandom.random_number(CODE_ALPHABET.length)] }.join
  end

  def acceptable?
    accepted_at.nil? && expires_at.future?
  end
end
