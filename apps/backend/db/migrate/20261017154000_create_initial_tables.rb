class CreateInitialTables < ActiveRecord::Migration[8.1]
  def change
    create_table :users do |t|
      t.string :email, null: false
      t.string :password_digest, null: false

      t.timestamps
    end
    add_index :users, :email, unique: true

    create_table :devices, id: :string do |t|
      t.datetime :last_seen_at

      t.timestamps
    end

    create_table :device_settings do |t|
      t.string :device_id, null: false
      t.boolean :leave_greeting_enabled, null: false, default: true
      t.boolean :arrive_greeting_enabled, null: false, default: true
      t.integer :volume, null: false, default: 70
      t.integer :leave_audio_version, null: false, default: 1
      t.integer :arrive_audio_version, null: false, default: 1

      t.timestamps
    end
    add_index :device_settings, :device_id, unique: true
    add_foreign_key :device_settings, :devices

    create_table :home_events, id: :string do |t|
      t.string :device_id, null: false
      t.string :event_type, null: false
      t.datetime :occurred_at, null: false

      t.timestamps
    end
    add_index :home_events, :device_id
    add_index :home_events, [ :device_id, :occurred_at ]
    add_foreign_key :home_events, :devices
  end
end
