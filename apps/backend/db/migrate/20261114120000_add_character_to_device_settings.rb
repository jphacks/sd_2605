class AddCharacterToDeviceSettings < ActiveRecord::Migration[8.1]
  def change
    add_column :device_settings, :character, :jsonb
    add_column :device_settings, :character_version, :integer, null: false, default: 0
  end
end
