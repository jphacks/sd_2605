class CreateWebPushNotifications < ActiveRecord::Migration[8.1]
  def change
    create_table :push_subscriptions do |t|
      t.references :user, null: false, foreign_key: true
      t.text :endpoint, null: false
      t.string :endpoint_digest, null: false
      t.string :p256dh, null: false
      t.string :auth, null: false
      t.timestamps
    end
    add_index :push_subscriptions, :endpoint_digest, unique: true

    create_table :push_deliveries do |t|
      t.references :home_event, type: :string, null: false, foreign_key: true
      t.references :push_subscription, null: false, foreign_key: true
      t.integer :attempts, null: false, default: 0
      t.datetime :sent_at
      t.datetime :canceled_at
      t.datetime :next_attempt_at
      t.timestamps
    end
    add_index :push_deliveries, [ :home_event_id, :push_subscription_id ], unique: true, name: "index_push_deliveries_on_event_and_subscription"
    add_index :push_deliveries, [ :sent_at, :next_attempt_at ]
  end
end
