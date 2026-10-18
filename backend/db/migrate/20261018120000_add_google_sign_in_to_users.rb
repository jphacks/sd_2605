class AddGoogleSignInToUsers < ActiveRecord::Migration[8.1]
  def change
    add_column :users, :google_sub, :string
    add_index :users, :google_sub, unique: true
    change_column_null :users, :password_digest, true
  end
end
