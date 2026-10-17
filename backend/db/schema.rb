# This file is auto-generated from the current state of the database. Instead
# of editing this file, please use the migrations feature of Active Record to
# incrementally modify your database, and then regenerate this schema definition.
#
# This file is the source Rails uses to define your schema when running `bin/rails
# db:schema:load`. When creating a new database, `bin/rails db:schema:load` tends to
# be faster and is potentially less error prone than running all of your
# migrations from scratch. Old migrations may fail to apply correctly if those
# migrations use external dependencies or application code.
#
# It's strongly recommended that you check this file into your version control system.

ActiveRecord::Schema[8.1].define(version: 2026_10_17_162000) do
  # These are extensions that must be enabled in order to support this database
  enable_extension "pg_catalog.plpgsql"

  create_table "device_settings", force: :cascade do |t|
    t.string "device_id", null: false
    t.boolean "leave_greeting_enabled", default: true, null: false
    t.boolean "arrive_greeting_enabled", default: true, null: false
    t.integer "volume", default: 70, null: false
    t.integer "leave_audio_version", default: 1, null: false
    t.integer "arrive_audio_version", default: 1, null: false
    t.datetime "created_at", null: false
    t.datetime "updated_at", null: false
    t.string "message_registered_by"
    t.datetime "message_uploaded_at"
    t.index ["device_id"], name: "index_device_settings_on_device_id", unique: true
  end

  create_table "devices", id: :string, force: :cascade do |t|
    t.datetime "last_seen_at"
    t.datetime "created_at", null: false
    t.datetime "updated_at", null: false
    t.string "password_digest"
  end

  create_table "home_events", id: :string, force: :cascade do |t|
    t.string "device_id", null: false
    t.string "event_type", null: false
    t.datetime "occurred_at", null: false
    t.datetime "created_at", null: false
    t.datetime "updated_at", null: false
    t.index ["device_id", "occurred_at"], name: "index_home_events_on_device_id_and_occurred_at"
    t.index ["device_id"], name: "index_home_events_on_device_id"
  end

  create_table "users", force: :cascade do |t|
    t.string "email", null: false
    t.string "password_digest", null: false
    t.datetime "created_at", null: false
    t.datetime "updated_at", null: false
    t.index ["email"], name: "index_users_on_email", unique: true
  end

  add_foreign_key "device_settings", "devices"
  add_foreign_key "home_events", "devices"
end
