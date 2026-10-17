class AddWebAuthenticationAndMessageMetadata < ActiveRecord::Migration[8.1]
  def change
    add_column :devices, :password_digest, :string
    add_column :device_settings, :message_registered_by, :string
    add_column :device_settings, :message_uploaded_at, :datetime
  end
end
