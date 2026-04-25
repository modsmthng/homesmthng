#pragma once

// Shared application state for the firmware.
//
// The project still uses LVGL/HomeSpan callbacks that need access to common
// runtime objects. Keeping those declarations in one place makes future module
// splits safer and avoids hidden duplicate state.

#include <Arduino.h>
#include <DNSServer.h>
#include <HomeSpan.h>
#include <Preferences.h>
#include <WebServer.h>
#include <lvgl.h>

#include "board_profile.h"
#include "display_backend.h"

constexpr int kNumSwitches = 9;
constexpr int kNumUiSwitches = 8;
constexpr int kNumColors = 6;
constexpr int kNumTimezones = 9;
constexpr int kTimezoneVisibleRows = 5;
constexpr int kWebBrightnessScaleMax = 10;
constexpr int kDefaultBrightnessScale = 8;
constexpr uint32_t kDefaultColorHex = 0xF5F1E8;
constexpr uint32_t kClockSaverIdleMs = 60000;
constexpr uint32_t kClockSaverMinIdleMs = 10000;
constexpr uint32_t kClockSaverMaxIdleMs = 3600000;
constexpr uint32_t kClockWakeDetectMs = 250;
constexpr uint32_t kPixelShiftIntervalMs = 30000;
constexpr uint32_t kWeatherRefreshMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kWeatherRetryMs = 60UL * 1000UL;
constexpr uint32_t kDeferredRebootMs = 1500;
constexpr int kPixelShiftAmplitude = 2;
constexpr time_t kValidClockEpoch = 1704067200; // 2024-01-01 00:00:00 UTC
constexpr int kDefaultTimezoneIndex = 1;
constexpr int kWeatherSearchResultLimit = 5;
constexpr uint16_t kAdminWebPort = 8080;
constexpr uint16_t kProvisioningPort = 80;
constexpr int kDefaultDisplayRotationDegrees = 0;
constexpr size_t kPixelShiftStepCount = 9;

extern const char kHomeKitSetupId[];
extern const char kHomeSpanWifiNamespace[];
extern const char kHomeSpanWifiKey[];
extern const char kDefaultHostBase[];
extern const int8_t kPixelShiftSteps[][2];
extern const char *const kSwitchNames[kNumSwitches];
extern const uint32_t kColorHexOptions[kNumColors];
extern const char *const kColorLabels[kNumColors];
extern const char *const kTimezoneLabels[kNumTimezones];
extern const char *const kTimezoneRules[kNumTimezones];
extern const char kTimezoneDropdownOptions[];

struct WeatherLocation {
    float latitude;
    float longitude;
};

struct WeatherSearchResult {
    String name;
    String admin1;
    String country;
    float latitude = 0.0f;
    float longitude = 0.0f;
    bool valid = false;
};

struct StoredWifiCredentials {
    char ssid[MAX_SSID + 1] = "";
    char pwd[MAX_PWD + 1] = "";
};

enum class WeatherGlyph {
    Sun,
    Cloud,
    Rain,
};

extern const WeatherLocation kWeatherLocations[kNumTimezones];

extern Preferences preferences;

extern String HOMEKIT_PAIRING_CODE_STR;
extern const char *HOMEKIT_PAIRING_CODE;
extern String BRIDGE_SUFFIX_STR;
extern String weather_location_name;
extern String device_host_name;
extern String device_label;

extern lv_obj_t *ui_root;
extern lv_obj_t *ui_viewport;
extern lv_obj_t *ui_shift_layer;
extern lv_obj_t *tileview;
extern lv_obj_t *tileMain;
extern lv_obj_t *tileSecond;
extern lv_obj_t *tileMaster;
extern lv_obj_t *tileMasterClock;
extern lv_obj_t *tileSett;
extern lv_obj_t *tileDisplayCare;
extern lv_obj_t *tileCode;
extern lv_obj_t *ui_switch_buttons[kNumUiSwitches];
extern lv_obj_t *ui_big_button;
extern lv_obj_t *ui_master_clock_button;
extern lv_obj_t *ui_master_clock_info;
extern lv_obj_t *ui_wifi_label;
extern lv_obj_t *ui_settings_web_links_label;
extern lv_obj_t *ui_brightness_slider;
extern lv_obj_t *ui_bl_toggle;
extern lv_obj_t *ui_wifi_setup_msg;
extern lv_obj_t *ui_pixel_shift_toggle;
extern lv_obj_t *ui_clock_button_toggle;
extern lv_obj_t *ui_clock_saver_toggle;
extern lv_obj_t *ui_timezone_button;
extern lv_obj_t *ui_timezone_value_label;
extern lv_obj_t *ui_timezone_status_label;
extern lv_obj_t *ui_timezone_modal;
extern lv_obj_t *ui_timezone_roller;
extern lv_obj_t *ui_timezone_done_button;
extern lv_obj_t *ui_weather_setup_button;
extern lv_obj_t *ui_weather_setup_value_label;
extern lv_obj_t *ui_weather_setup_modal;
extern lv_obj_t *ui_weather_setup_modal_text;
extern lv_obj_t *ui_weather_setup_done_button;
extern lv_obj_t *screensaver_overlay;
extern lv_obj_t *screensaver_clock_layer;
extern lv_obj_t *screensaver_clock_face;
extern lv_obj_t *screensaver_hour_hand;
extern lv_obj_t *screensaver_minute_hand;
extern lv_obj_t *screensaver_second_hand;
extern lv_obj_t *screensaver_center_dot;
extern lv_obj_t *screensaver_weather_group;
extern lv_obj_t *screensaver_weather_icon;
extern lv_obj_t *screensaver_weather_temp_label;
extern lv_obj_t *screensaver_weather_sun_core;
extern lv_obj_t *screensaver_weather_sun_rays[8];
extern lv_obj_t *screensaver_weather_cloud_parts[4];
extern lv_obj_t *screensaver_weather_rain_lines[3];
extern lv_obj_t *master_clock_face;
extern lv_obj_t *master_hour_hand;
extern lv_obj_t *master_minute_hand;
extern lv_obj_t *master_second_hand;
extern lv_obj_t *master_center_dot;
extern lv_obj_t *master_weather_group;
extern lv_obj_t *master_weather_icon;
extern lv_obj_t *master_weather_temp_label;
extern lv_obj_t *master_weather_sun_core;
extern lv_obj_t *master_weather_sun_rays[8];
extern lv_obj_t *master_weather_cloud_parts[4];
extern lv_obj_t *master_weather_rain_lines[3];

extern lv_point_t screensaver_hour_points[2];
extern lv_point_t screensaver_minute_points[2];
extern lv_point_t screensaver_second_points[2];
extern lv_point_t screensaver_weather_sun_ray_points[8][2];
extern lv_point_t master_hour_points[2];
extern lv_point_t master_minute_points[2];
extern lv_point_t master_second_points[2];
extern lv_point_t master_weather_sun_ray_points[8][2];

extern lv_timer_t *screen2_timer;
extern WebServer weather_config_server;
extern WebServer provisioning_server;
extern DNSServer provisioning_dns_server;

extern int global_brightness;
extern int current_display_brightness;
extern int display_ui_rotation_degrees;
extern bool screen2_bl_always_on;
extern bool oled_pixel_shift_enabled;
extern bool oled_clock_saver_enabled;
extern bool clock_button_screen_enabled;
extern bool clock_button_screen_first;
extern uint32_t clock_saver_idle_ms;
extern bool screensaver_visible;
extern int pixel_shift_x;
extern int pixel_shift_y;
extern int timezone_index;
extern bool timezone_sync_pending;
extern bool weather_use_custom_location;
extern bool weather_enabled;
extern bool weather_has_data;
extern bool weather_refresh_pending;
extern int weather_temperature_c;
extern int weather_code;
extern bool weather_is_day;
extern float weather_custom_latitude;
extern float weather_custom_longitude;
extern unsigned long weather_last_request_ms;
extern unsigned long weather_last_success_ms;
extern WeatherGlyph weather_glyph;
extern bool weather_config_server_routes_registered;
extern bool weather_config_server_running;
extern bool provisioning_server_routes_registered;
extern bool provisioning_server_running;
extern bool provisioning_mode_active;
extern bool wifi_setup_info_visible;
extern bool homespan_started;
extern bool homekit_is_paired;
extern bool reboot_scheduled;
extern unsigned long reboot_scheduled_at;

extern uint32_t active_switch_color_hex;
extern lv_color_t active_switch_color;

extern uint8_t switch_button_ids[kNumUiSwitches];
extern uint8_t color_button_ids[kNumColors];

DisplayBackend &displayBackend();
const BoardProfile &boardProfile();
const UiMetrics &uiMetrics();
lv_coord_t scaleUi(int base_px);

void safeSetBrightness(int target_brightness);
bool supportsPixelShift();
bool supportsClockSaver();
bool isPixelShiftEnabled();
bool isClockSaverEnabled();
bool isClockButtonEnabled();
bool isWeatherEnabled();
bool shouldShowClockWeather();
int screensaverBrightness();
int pixelShiftSafeCropInset();
bool isValidWeatherCoordinates(float latitude, float longitude);
bool hasCustomWeatherLocation();
