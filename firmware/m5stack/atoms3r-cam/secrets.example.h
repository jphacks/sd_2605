// Copy to secrets.h (Git ignored). Register this ID separately from CoreS3.
#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-password"
#define RAILS_BASE_URL "https://sd-2605-mobile.onrender.com"
#define ENTRANCE_DEVICE_ID "atoms3r-cam-1"
// First provisioning ONLY: true formats LittleFS if mounting fails.
// Set back to false after initial provisioning to retain damaged FS for diagnosis.
#define ATOM_FORMAT_FS_ON_FAILURE false
// Reverse to false if the installation faces the other direction.
#define ATOM_LEFT_TO_RIGHT_IS_LEAVE true
