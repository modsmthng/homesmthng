// Shared application state and small hardware-facing helpers.
//
// Most LVGL and HomeSpan callbacks are still split incrementally. The state
// lives here so each extracted module can use the same runtime objects without
// creating duplicate globals.

#include "app_state.h"

#include <memory>

const char kHomeKitSetupId[] = "HSPN";
const char kHomeSpanWifiNamespace[] = "WIFI";
const char kHomeSpanWifiKey[] = "WIFIDATA";
const char kDefaultHostBase[] = "homesmthng";

const int8_t kPixelShiftSteps[][2] = {
    {0, 0},
    {1, 0},
    {1, 1},
    {0, 1},
    {-1, 1},
    {-1, 0},
    {-1, -1},
    {0, -1},
    {1, -1},
};

const char *const kSwitchNames[kNumSwitches] = {"S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8", "B1"};
const uint32_t kColorHexOptions[kNumColors] = {0xFF0000, 0xFF9900, 0xF5F1E8, 0xA8C6FE, 0x68724D, 0xF99963};
const char *const kColorLabels[kNumColors] = {"Red", "Orange", "Warm White", "Blue", "Olive", "Peach"};
const char *const kTimezoneLabels[kNumTimezones] = {
    "UTC",
    "Berlin",
    "London",
    "New York",
    "Chicago",
    "Denver",
    "Los Angeles",
    "Tokyo",
    "Sydney",
};
const char *const kTimezoneRules[kNumTimezones] = {
    "UTC0",
    "CET-1CEST,M3.5.0/2,M10.5.0/3",
    "GMT0BST,M3.5.0/1,M10.5.0/2",
    "EST5EDT,M3.2.0/2,M11.1.0/2",
    "CST6CDT,M3.2.0/2,M11.1.0/2",
    "MST7MDT,M3.2.0/2,M11.1.0/2",
    "PST8PDT,M3.2.0/2,M11.1.0/2",
    "JST-9",
    "AEST-10AEDT,M10.1.0/2,M4.1.0/3",
};
const char kTimezoneDropdownOptions[] =
    "UTC\n"
    "Berlin\n"
    "London\n"
    "New York\n"
    "Chicago\n"
    "Denver\n"
    "Los Angeles\n"
    "Tokyo\n"
    "Sydney";

const WeatherLocation kWeatherLocations[kNumTimezones] = {
    {51.4779f, 0.0015f},
    {52.5200f, 13.4050f},
    {51.5072f, -0.1276f},
    {40.7128f, -74.0060f},
    {41.8781f, -87.6298f},
    {39.7392f, -104.9903f},
    {34.0522f, -118.2437f},
    {35.6762f, 139.6503f},
    {-33.8688f, 151.2093f},
};

Preferences preferences;

String HOMEKIT_PAIRING_CODE_STR = "22446688";
const char *HOMEKIT_PAIRING_CODE = nullptr;
String BRIDGE_SUFFIX_STR;
String weather_location_name;
String device_host_name;
String device_label;

lv_obj_t *ui_root = nullptr;
lv_obj_t *ui_viewport = nullptr;
lv_obj_t *ui_shift_layer = nullptr;
lv_obj_t *tileview = nullptr;
lv_obj_t *tileMain = nullptr;
lv_obj_t *tileSecond = nullptr;
lv_obj_t *tileMaster = nullptr;
lv_obj_t *tileMasterClock = nullptr;
lv_obj_t *tileSett = nullptr;
lv_obj_t *tileDisplayCare = nullptr;
lv_obj_t *tileCode = nullptr;
lv_obj_t *ui_switch_buttons[kNumUiSwitches] = {};
lv_obj_t *ui_big_button = nullptr;
lv_obj_t *ui_master_clock_button = nullptr;
lv_obj_t *ui_master_clock_info = nullptr;
lv_obj_t *ui_wifi_label = nullptr;
lv_obj_t *ui_settings_web_links_label = nullptr;
lv_obj_t *ui_brightness_slider = nullptr;
lv_obj_t *ui_bl_toggle = nullptr;
lv_obj_t *ui_wifi_setup_msg = nullptr;
lv_obj_t *ui_pixel_shift_toggle = nullptr;
lv_obj_t *ui_clock_button_toggle = nullptr;
lv_obj_t *ui_clock_saver_toggle = nullptr;
lv_obj_t *ui_timezone_button = nullptr;
lv_obj_t *ui_timezone_value_label = nullptr;
lv_obj_t *ui_timezone_status_label = nullptr;
lv_obj_t *ui_timezone_modal = nullptr;
lv_obj_t *ui_timezone_roller = nullptr;
lv_obj_t *ui_timezone_done_button = nullptr;
lv_obj_t *ui_weather_setup_button = nullptr;
lv_obj_t *ui_weather_setup_value_label = nullptr;
lv_obj_t *ui_weather_setup_modal = nullptr;
lv_obj_t *ui_weather_setup_modal_text = nullptr;
lv_obj_t *ui_weather_setup_done_button = nullptr;
lv_obj_t *screensaver_overlay = nullptr;
lv_obj_t *screensaver_clock_layer = nullptr;
lv_obj_t *screensaver_clock_face = nullptr;
lv_obj_t *screensaver_hour_hand = nullptr;
lv_obj_t *screensaver_minute_hand = nullptr;
lv_obj_t *screensaver_second_hand = nullptr;
lv_obj_t *screensaver_center_dot = nullptr;
lv_obj_t *screensaver_weather_group = nullptr;
lv_obj_t *screensaver_weather_icon = nullptr;
lv_obj_t *screensaver_weather_temp_label = nullptr;
lv_obj_t *screensaver_weather_sun_core = nullptr;
lv_obj_t *screensaver_weather_sun_rays[8] = {};
lv_obj_t *screensaver_weather_cloud_parts[4] = {};
lv_obj_t *screensaver_weather_rain_lines[3] = {};
lv_obj_t *master_clock_face = nullptr;
lv_obj_t *master_hour_hand = nullptr;
lv_obj_t *master_minute_hand = nullptr;
lv_obj_t *master_second_hand = nullptr;
lv_obj_t *master_center_dot = nullptr;
lv_obj_t *master_weather_group = nullptr;
lv_obj_t *master_weather_icon = nullptr;
lv_obj_t *master_weather_temp_label = nullptr;
lv_obj_t *master_weather_sun_core = nullptr;
lv_obj_t *master_weather_sun_rays[8] = {};
lv_obj_t *master_weather_cloud_parts[4] = {};
lv_obj_t *master_weather_rain_lines[3] = {};

lv_point_t screensaver_hour_points[2] = {};
lv_point_t screensaver_minute_points[2] = {};
lv_point_t screensaver_second_points[2] = {};
lv_point_t screensaver_weather_sun_ray_points[8][2] = {};
lv_point_t master_hour_points[2] = {};
lv_point_t master_minute_points[2] = {};
lv_point_t master_second_points[2] = {};
lv_point_t master_weather_sun_ray_points[8][2] = {};

lv_timer_t *screen2_timer = nullptr;
WebServer weather_config_server(kAdminWebPort);
WebServer provisioning_server(kProvisioningPort);
DNSServer provisioning_dns_server;

int global_brightness = 13;
int current_display_brightness = -1;
int display_ui_rotation_degrees = kDefaultDisplayRotationDegrees;
bool screen2_bl_always_on = true;
bool oled_pixel_shift_enabled = false;
bool oled_clock_saver_enabled = true;
bool clock_button_screen_enabled = true;
bool clock_button_screen_first = false;
uint32_t clock_saver_idle_ms = kClockSaverIdleMs;
bool screensaver_visible = false;
int pixel_shift_x = 0;
int pixel_shift_y = 0;
int timezone_index = kDefaultTimezoneIndex;
bool timezone_sync_pending = true;
bool weather_use_custom_location = false;
bool weather_enabled = true;
bool weather_has_data = false;
bool weather_refresh_pending = true;
int weather_temperature_c = 0;
int weather_code = 0;
bool weather_is_day = true;
float weather_custom_latitude = 0.0f;
float weather_custom_longitude = 0.0f;
unsigned long weather_last_request_ms = 0;
unsigned long weather_last_success_ms = 0;
WeatherGlyph weather_glyph = WeatherGlyph::Cloud;
bool weather_config_server_routes_registered = false;
bool weather_config_server_running = false;
bool provisioning_server_routes_registered = false;
bool provisioning_server_running = false;
bool provisioning_mode_active = false;
bool wifi_setup_info_visible = false;
bool homespan_started = false;
bool homekit_is_paired = false;
bool reboot_scheduled = false;
unsigned long reboot_scheduled_at = 0;

uint32_t active_switch_color_hex = kDefaultColorHex;
lv_color_t active_switch_color = lv_color_hex(active_switch_color_hex);

uint8_t switch_button_ids[kNumUiSwitches] = {};
uint8_t color_button_ids[kNumColors] = {};

DisplayBackend &displayBackend()
{
    static std::unique_ptr<DisplayBackend> backend = createDisplayBackend();
    return *backend;
}

const BoardProfile &boardProfile()
{
    return displayBackend().profile();
}

const UiMetrics &uiMetrics()
{
    return boardProfile().ui;
}

lv_coord_t scaleUi(int base_px)
{
    return static_cast<lv_coord_t>(uiMetrics().scale(base_px));
}

void safeSetBrightness(int target_brightness)
{
    if (target_brightness < 1) {
        target_brightness = 0;
    }
    if (target_brightness > boardProfile().brightness_levels) {
        target_brightness = boardProfile().brightness_levels;
    }

    if (target_brightness != current_display_brightness) {
        displayBackend().setBrightness(static_cast<uint8_t>(target_brightness));
        current_display_brightness = target_brightness;
    }
}

bool supportsPixelShift()
{
    return true;
}

bool supportsClockSaver()
{
    return true;
}

bool isPixelShiftEnabled()
{
    return supportsPixelShift() && oled_pixel_shift_enabled;
}

bool isClockSaverEnabled()
{
    return supportsClockSaver() && oled_clock_saver_enabled;
}

bool isClockButtonEnabled()
{
    return supportsClockSaver() && clock_button_screen_enabled;
}

bool isWeatherEnabled()
{
    return weather_enabled && hasCustomWeatherLocation();
}

bool shouldShowClockWeather()
{
    return isWeatherEnabled() && weather_has_data;
}

int screensaverBrightness()
{
    int target = global_brightness;
    if (target < 0) {
        target = 0;
    }
    if (target > boardProfile().brightness_levels) {
        target = boardProfile().brightness_levels;
    }
    return target;
}

int pixelShiftSafeCropInset()
{
    return kPixelShiftAmplitude;
}

bool isValidWeatherCoordinates(float latitude, float longitude)
{
    return latitude >= -90.0f && latitude <= 90.0f && longitude >= -180.0f && longitude <= 180.0f;
}

bool hasCustomWeatherLocation()
{
    return weather_use_custom_location && isValidWeatherCoordinates(weather_custom_latitude, weather_custom_longitude);
}
