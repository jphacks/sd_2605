class AddUserToDevices < ActiveRecord::Migration[8.1]
  def change
    add_reference :devices, :user, foreign_key: true
  end
end
