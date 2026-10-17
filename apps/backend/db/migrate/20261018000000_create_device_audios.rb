class CreateDeviceAudios < ActiveRecord::Migration[8.1]
  def change
    create_table :device_audios do |t|
      t.string :device_id, null: false
      t.string :slot, null: false
      t.binary :wav, null: false
      t.string :registered_by
      t.timestamps
    end
    add_index :device_audios, [ :device_id, :slot ], unique: true
    add_foreign_key :device_audios, :devices
  end
end
