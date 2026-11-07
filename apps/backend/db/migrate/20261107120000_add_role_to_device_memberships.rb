class AddRoleToDeviceMemberships < ActiveRecord::Migration[8.1]
  def change
    add_column :device_memberships, :role, :string, null: false, default: "resident"
  end
end
