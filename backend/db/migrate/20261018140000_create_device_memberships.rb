class CreateDeviceMemberships < ActiveRecord::Migration[8.1]
  def change
    create_table :device_memberships do |t|
      t.references :user, null: false, foreign_key: true, index: false
      t.references :device, null: false, foreign_key: true, type: :string, index: false
      t.timestamps
    end
    add_index :device_memberships, [ :user_id, :device_id ], unique: true
    add_index :device_memberships, :device_id

    reversible do |dir|
      dir.up do
        execute <<~SQL.squish
          INSERT INTO device_memberships (user_id, device_id, created_at, updated_at)
          SELECT user_id, id, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP
          FROM devices
          WHERE user_id IS NOT NULL
        SQL
      end
    end
  end
end
