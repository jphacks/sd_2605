# This file should ensure the existence of records required to run the application in every environment (production,
# development, test). The code here should be idempotent so that it can be executed at any point in every environment.
# The data can then be loaded with the bin/rails db:seed command (or created alongside the database with db:setup).
#
device = Device.find_by(id: "entrance-1")
if !device && ENV["DEMO_DEVICE_PASSWORD"].present?
  device = Device.create!(id: "entrance-1", password: ENV.fetch("DEMO_DEVICE_PASSWORD"))
end
DeviceSetting.find_or_create_by!(device: device) if device
