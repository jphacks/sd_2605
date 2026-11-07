class CreateHomeEventVoices < ActiveRecord::Migration[8.1]
  def change
    add_column :home_events, :voice_skipped, :boolean, null: false, default: false
    add_column :home_events, :messengers_notified_at, :datetime

    create_table :home_event_voices do |t|
      t.string :home_event_id, null: false
      t.binary :wav, null: false
      t.timestamps
    end
    add_index :home_event_voices, :home_event_id, unique: true
    add_foreign_key :home_event_voices, :home_events
  end
end
