class AddUserToDeviceAudios < ActiveRecord::Migration[8.1]
  def change
    add_reference :device_audios, :user, foreign_key: { on_delete: :nullify }
  end
end
