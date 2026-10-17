require "digest"

# One-time administrator-authorized repair of the original unowned demo record.
# The account is matched by digest so its email is not published in repository history.
class LinkLegacyEntranceDeviceToAccount < ActiveRecord::Migration[8.1]
  class LegacyDevice < ActiveRecord::Base
    self.table_name = "devices"
  end

  class ExistingUser < ActiveRecord::Base
    self.table_name = "users"
  end

  def target_account_digest
    "9e293bdd56e59c560d7db72578ace0ff2c3a1fc67dd955c2e8dacc6c262ba0b5"
  end

  def up
    device = LegacyDevice.find_by(id: "entrance-1")
    return unless device
    owner = ExistingUser.select(:id, :email).find_each.detect do |user|
      Digest::SHA256.hexdigest(user.email.to_s.strip.downcase) == target_account_digest
    end
    unless owner
      say "Skipped legacy device repair: target account does not exist"
      return
    end
    device.with_lock do
      if device.user_id.present? || device.password_digest.present?
        say "Skipped legacy device repair: ownership or credentials already configured"
        next
      end
      device.update!(user_id: owner.id)
      say "Linked legacy entrance-1 to its designated existing account"
    end
  end

  def down
    raise ActiveRecord::IrreversibleMigration, "Account ownership must not be automatically reassigned"
  end
end
