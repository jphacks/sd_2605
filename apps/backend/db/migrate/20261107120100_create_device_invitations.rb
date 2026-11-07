class CreateDeviceInvitations < ActiveRecord::Migration[8.1]
  def change
    create_table :device_invitations do |t|
      t.references :device, null: false, foreign_key: true, type: :string
      t.references :invited_by, null: false, foreign_key: { to_table: :users }
      t.string :code, null: false
      t.datetime :expires_at, null: false
      t.references :accepted_by, foreign_key: { to_table: :users }
      t.datetime :accepted_at
      t.timestamps
    end
    add_index :device_invitations, :code, unique: true
  end
end
