#pragma once
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef RAILS_BASE_URL
#define RAILS_BASE_URL "https://sd-2605-mobile.onrender.com"
#endif
#ifndef ENTRANCE_DEVICE_ID
#define ENTRANCE_DEVICE_ID "atoms3r-cam-1"
#endif
#ifndef ATOM_FORMAT_FS_ON_FAILURE
#define ATOM_FORMAT_FS_ON_FAILURE false
#endif
#ifndef API_TLS_INSECURE
#define API_TLS_INSECURE false
#endif
namespace Config {
constexpr char baseUrl[] = RAILS_BASE_URL, deviceId[] = ENTRANCE_DEVICE_ID;
constexpr uint32_t heartbeatMs = 20000, maxWav = 200 * 1024;
constexpr bool formatIfMountFails = ATOM_FORMAT_FS_ON_FAILURE;
}
