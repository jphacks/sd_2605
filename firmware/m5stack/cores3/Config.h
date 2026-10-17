#pragma once
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif
#ifndef RAILS_BASE_URL
#define RAILS_BASE_URL "https://sd-2605-mobile.onrender.com"
#endif
#ifndef ENTRANCE_DEVICE_ID
#define ENTRANCE_DEVICE_ID "entrance-1"
#endif
#ifndef ENABLE_AUTO_EVENTS
#define ENABLE_AUTO_EVENTS false
#endif
#ifndef API_TLS_INSECURE
#define API_TLS_INSECURE false
#endif
namespace Config {
constexpr char baseUrl[] = RAILS_BASE_URL, deviceId[] = ENTRANCE_DEVICE_ID;
constexpr uint32_t heartbeatMs = 20000, cooldownMs = 10000, sampleMs = 75;
constexpr uint32_t maxWav = 200 * 1024;
// Baseline-relative offsets: measure on site; these are initial tuning values.
constexpr uint16_t nearDelta = 150, releaseDelta = 70;
constexpr bool formatIfMountFails = false;  // Explicit opt-in only: erases damaged/unformatted FS.
}  // namespace Config
