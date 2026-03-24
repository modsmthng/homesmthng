/**
 * @file      main.cpp
 * @author    modsmthng
 * @date      2026-03-23
 */

#include <Arduino.h>
#include <HTTPClient.h>
#include <HomeSpan.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <lvgl.h>

#include <math.h>
#include <memory>
#include <time.h>

#include "display_backend.h"

namespace {

constexpr int kNumSwitches = 9;
constexpr int kNumUiSwitches = 8;
constexpr int kNumColors = 6;
constexpr int kNumTimezones = 9;
constexpr int kTimezoneVisibleRows = 5;
constexpr uint32_t kClockSaverIdleMs = 45000;
constexpr uint32_t kClockWakeDetectMs = 250;
constexpr uint32_t kPixelShiftIntervalMs = 30000;
constexpr uint32_t kWeatherRefreshMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kWeatherRetryMs = 60UL * 1000UL;
constexpr int kPixelShiftAmplitude = 2;
constexpr time_t kValidClockEpoch = 1704067200; // 2024-01-01 00:00:00 UTC
constexpr int kDefaultTimezoneIndex = 1;

constexpr int8_t kPixelShiftSteps[][2] = {
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

const char *kSwitchNames[kNumSwitches] = {"S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8", "B1"};
const uint32_t kColorHexOptions[kNumColors] = {0xFF0000, 0xFF9900, 0xCCCCCC, 0x0000FF, 0x68724D, 0xF99963};
const char *kTimezoneLabels[kNumTimezones] = {
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
const char *kTimezoneRules[kNumTimezones] = {
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
constexpr char kTimezoneDropdownOptions[] =
    "UTC\n"
    "Berlin\n"
    "London\n"
    "New York\n"
    "Chicago\n"
    "Denver\n"
    "Los Angeles\n"
    "Tokyo\n"
    "Sydney";

struct WeatherLocation {
    float latitude;
    float longitude;
};

enum class WeatherGlyph {
    Sun,
    Cloud,
    Rain,
};

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

class MySwitch;

Preferences preferences;
MySwitch *homekit_switches[kNumSwitches] = {};

String HOMEKIT_PAIRING_CODE_STR = "22446688";
const char *HOMEKIT_PAIRING_CODE = nullptr;
String BRIDGE_SUFFIX_STR;

lv_obj_t *ui_root = nullptr;
lv_obj_t *ui_viewport = nullptr;
lv_obj_t *ui_shift_layer = nullptr;
lv_obj_t *tileview = nullptr;
lv_obj_t *tileMain = nullptr;
lv_obj_t *tileSecond = nullptr;
lv_obj_t *tileMaster = nullptr;
lv_obj_t *tileSett = nullptr;
lv_obj_t *tileDisplayCare = nullptr;
lv_obj_t *tileCode = nullptr;
lv_obj_t *ui_switch_buttons[kNumUiSwitches] = {};
lv_obj_t *ui_big_button = nullptr;
lv_obj_t *ui_wifi_label = nullptr;
lv_obj_t *ui_brightness_slider = nullptr;
lv_obj_t *ui_bl_toggle = nullptr;
lv_obj_t *ui_wifi_setup_msg = nullptr;
lv_obj_t *ui_pixel_shift_toggle = nullptr;
lv_obj_t *ui_clock_saver_toggle = nullptr;
lv_obj_t *ui_timezone_button = nullptr;
lv_obj_t *ui_timezone_value_label = nullptr;
lv_obj_t *ui_timezone_status_label = nullptr;
lv_obj_t *ui_timezone_modal = nullptr;
lv_obj_t *ui_timezone_roller = nullptr;
lv_obj_t *ui_timezone_done_button = nullptr;
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
lv_obj_t *screensaver_weather_sun_rays[4] = {};
lv_obj_t *screensaver_weather_cloud_parts[4] = {};
lv_obj_t *screensaver_weather_rain_lines[3] = {};

lv_point_t screensaver_hour_points[2] = {};
lv_point_t screensaver_minute_points[2] = {};
lv_point_t screensaver_second_points[2] = {};

lv_timer_t *screen2_timer = nullptr;

int global_brightness = 12;
int current_display_brightness = -1;
bool screen2_bl_always_on = true;
bool oled_pixel_shift_enabled = false;
bool oled_clock_saver_enabled = false;
bool screensaver_visible = false;
int pixel_shift_x = 0;
int pixel_shift_y = 0;
int timezone_index = kDefaultTimezoneIndex;
bool timezone_sync_pending = true;
bool weather_has_data = false;
bool weather_refresh_pending = true;
int weather_temperature_c = 0;
int weather_code = 0;
bool weather_is_day = true;
unsigned long weather_last_request_ms = 0;
unsigned long weather_last_success_ms = 0;
WeatherGlyph weather_glyph = WeatherGlyph::Cloud;

uint32_t active_switch_color_hex = 0x68724D;
lv_color_t active_switch_color = lv_color_hex(active_switch_color_hex);

uint8_t switch_button_ids[kNumUiSwitches] = {};
uint8_t color_button_ids[kNumColors] = {};

void updateLVGLState(uint8_t id, bool state);
void updateWiFiSymbol();
void safeSetBrightness(int target_brightness);
void screen2_off_timer_cb(lv_timer_t *);
void applyClockAccentColor(lv_color_t color);
void hideScrollbarVisuals(lv_obj_t *obj);
void updateScrollbarModes();
bool supportsPixelShift();
bool supportsClockSaver();
bool isPixelShiftEnabled();
bool isClockSaverEnabled();
int screensaverBrightness();
int pixelShiftSafeCropInset();
const WeatherLocation &selectedWeatherLocation();
void syncScreen2IdleTimer();
void applyPixelShiftOffset(int shift_x, int shift_y);
void updateScreensaverClock(bool force);
void updateWeatherMonogram();
bool extractJsonNumberField(const String &json, const char *key, float &value);
WeatherGlyph glyphForWeatherCode(int code, bool is_day);
void setScreensaverWeatherGlyph(WeatherGlyph glyph);
bool fetchCurrentWeather();
void updateWeatherIfNeeded();
void hideScreensaver(bool user_wake);
void updateOledProtection();
void applyTimezoneSetting(bool request_sync);
void ensureTimeIsConfigured();
void updateTimezoneButtonLabel();
void updateTimezoneStatusLabel();
void openTimezoneModal();
void closeTimezoneModal();
void timezone_button_event_handler(lv_event_t *e);
void timezone_roller_event_handler(lv_event_t *e);
void timezone_done_event_handler(lv_event_t *e);

class MySwitch : public Service::Switch {
  public:
    explicit MySwitch(uint8_t id) : Service::Switch(), switchId(id)
    {
        on = new Characteristic::On(false);
    }

    bool update() override
    {
        updateLVGLState(switchId, on->getNewVal());
        return true;
    }

    uint8_t switchId;
    Characteristic::On *on = nullptr;
};

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

const char *selectedTimezoneLabel()
{
    if (timezone_index < 0 || timezone_index >= kNumTimezones) {
        timezone_index = kDefaultTimezoneIndex;
    }
    return kTimezoneLabels[timezone_index];
}

const char *selectedTimezoneRule()
{
    if (timezone_index < 0 || timezone_index >= kNumTimezones) {
        timezone_index = kDefaultTimezoneIndex;
    }
    return kTimezoneRules[timezone_index];
}

const WeatherLocation &selectedWeatherLocation()
{
    if (timezone_index < 0 || timezone_index >= kNumTimezones) {
        timezone_index = kDefaultTimezoneIndex;
    }
    return kWeatherLocations[timezone_index];
}

void applyClockAccentColor(lv_color_t color)
{
    if (screensaver_hour_hand) {
        lv_obj_set_style_line_color(screensaver_hour_hand, color, 0);
    }
    if (screensaver_minute_hand) {
        lv_obj_set_style_line_color(screensaver_minute_hand, color, 0);
    }
    if (screensaver_second_hand) {
        lv_obj_set_style_line_color(screensaver_second_hand, color, 0);
    }
    if (screensaver_center_dot) {
        lv_obj_set_style_bg_color(screensaver_center_dot, color, 0);
    }
    if (screensaver_weather_sun_core) {
        lv_obj_set_style_bg_color(screensaver_weather_sun_core, color, 0);
    }
    for (lv_obj_t *ray : screensaver_weather_sun_rays) {
        if (ray) {
            lv_obj_set_style_bg_color(ray, color, 0);
        }
    }
    for (lv_obj_t *cloud_part : screensaver_weather_cloud_parts) {
        if (cloud_part) {
            lv_obj_set_style_bg_color(cloud_part, color, 0);
        }
    }
    for (lv_obj_t *rain_line : screensaver_weather_rain_lines) {
        if (rain_line) {
            lv_obj_set_style_bg_color(rain_line, color, 0);
        }
    }
}

void hideScrollbarVisuals(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_width(obj, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(obj, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_left(obj, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_top(obj, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_SCROLLBAR);
}

void updateScrollbarModes()
{
    if (!tileview) {
        return;
    }

    hideScrollbarVisuals(tileview);

    if (tileMain) {
        hideScrollbarVisuals(tileMain);
    }
    if (tileSecond) {
        hideScrollbarVisuals(tileSecond);
    }
    if (tileMaster) {
        hideScrollbarVisuals(tileMaster);
    }
    if (tileSett) {
        hideScrollbarVisuals(tileSett);
    }
    if (tileDisplayCare) {
        hideScrollbarVisuals(tileDisplayCare);
    }
    if (tileCode) {
        hideScrollbarVisuals(tileCode);
    }
}

void syncScreen2IdleTimer()
{
    if (!tileview) {
        return;
    }

    lv_obj_t *active_tile = lv_tileview_get_tile_act(tileview);
    if (!screen2_bl_always_on && active_tile == tileMaster) {
        if (screen2_timer) {
            lv_timer_reset(screen2_timer);
        } else {
            screen2_timer = lv_timer_create(screen2_off_timer_cb, 5000, nullptr);
        }
    } else if (screen2_timer) {
        lv_timer_del(screen2_timer);
        screen2_timer = nullptr;
    }
}

void applyPixelShiftOffset(int shift_x, int shift_y)
{
    const int safe_crop = pixelShiftSafeCropInset();
    pixel_shift_x = constrain(shift_x, -safe_crop, safe_crop);
    pixel_shift_y = constrain(shift_y, -safe_crop, safe_crop);

    if (ui_root) {
        lv_obj_set_pos(ui_root, uiMetrics().canvas_x, uiMetrics().canvas_y);
    }

    if (ui_shift_layer) {
        lv_obj_set_pos(ui_shift_layer, -safe_crop + pixel_shift_x, -safe_crop + pixel_shift_y);
    }

    if (screensaver_clock_layer) {
        lv_obj_set_pos(screensaver_clock_layer, 0, 0);
    }

    if (screensaver_clock_face && screensaver_clock_layer) {
        const lv_coord_t face_w = lv_obj_get_width(screensaver_clock_face);
        const lv_coord_t face_h = lv_obj_get_height(screensaver_clock_face);
        const lv_coord_t layer_w = lv_obj_get_width(screensaver_clock_layer);
        const lv_coord_t layer_h = lv_obj_get_height(screensaver_clock_layer);
        const lv_coord_t base_x = (layer_w - face_w) / 2;
        const lv_coord_t base_y = (layer_h - face_h) / 2;
        lv_obj_set_pos(screensaver_clock_face, base_x, base_y);
    }
}

void updateTimezoneButtonLabel()
{
    if (!ui_timezone_value_label) {
        return;
    }

    lv_label_set_text_fmt(ui_timezone_value_label, "%s  " LV_SYMBOL_RIGHT, selectedTimezoneLabel());
}

bool extractJsonNumberField(const String &json, const char *key, float &value)
{
    const String pattern = String("\"") + key + "\":";
    int start = json.indexOf(pattern);
    if (start < 0) {
        return false;
    }

    start += pattern.length();
    while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) {
        ++start;
    }

    int end = start;
    while (end < json.length()) {
        const char c = json[end];
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
            ++end;
            continue;
        }
        break;
    }

    if (end == start) {
        return false;
    }

    value = json.substring(start, end).toFloat();
    return true;
}

void getScreensaverClockTime(int &hour, int &minute, int &second)
{
    const time_t now = time(nullptr);
    if (now >= kValidClockEpoch) {
        struct tm timeinfo = {};
        localtime_r(&now, &timeinfo);
        hour = timeinfo.tm_hour;
        minute = timeinfo.tm_min;
        second = timeinfo.tm_sec;
        return;
    }

    const uint32_t total_seconds = millis() / 1000U;
    hour = (10 + static_cast<int>((total_seconds / 3600U) % 24U)) % 24;
    minute = (10 + static_cast<int>((total_seconds / 60U) % 60U)) % 60;
    second = static_cast<int>(total_seconds % 60U);
}

void updateTimezoneStatusLabel()
{
    if (!ui_timezone_status_label) {
        return;
    }

    char time_buffer[16] = {};
    char status_buffer[64] = {};
    const time_t now = time(nullptr);

    if (now >= kValidClockEpoch) {
        struct tm timeinfo = {};
        localtime_r(&now, &timeinfo);
        strftime(time_buffer, sizeof(time_buffer), "%H:%M", &timeinfo);
        snprintf(status_buffer, sizeof(status_buffer), "%s | %s", selectedTimezoneLabel(), time_buffer);
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(status_buffer, sizeof(status_buffer), "%s | Syncing...", selectedTimezoneLabel());
    } else {
        snprintf(status_buffer, sizeof(status_buffer), "%s | Offline", selectedTimezoneLabel());
    }

    lv_label_set_text(ui_timezone_status_label, status_buffer);
}

WeatherGlyph glyphForWeatherCode(int code, bool)
{
    if (code <= 1) {
        return WeatherGlyph::Sun;
    }

    if (code == 2 || code == 3 || code == 45 || code == 48) {
        return WeatherGlyph::Cloud;
    }

    if ((code >= 51 && code <= 67) || (code >= 71 && code <= 77) || (code >= 80 && code <= 99)) {
        return WeatherGlyph::Rain;
    }

    return WeatherGlyph::Cloud;
}

void setScreensaverWeatherGlyph(WeatherGlyph glyph)
{
    weather_glyph = glyph;

    const bool show_sun = (glyph == WeatherGlyph::Sun);
    const bool show_cloud = (glyph == WeatherGlyph::Cloud || glyph == WeatherGlyph::Rain);
    const bool show_rain = (glyph == WeatherGlyph::Rain);

    if (screensaver_weather_sun_core) {
        if (show_sun) {
            lv_obj_clear_flag(screensaver_weather_sun_core, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(screensaver_weather_sun_core, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (lv_obj_t *ray : screensaver_weather_sun_rays) {
        if (!ray) {
            continue;
        }
        if (show_sun) {
            lv_obj_clear_flag(ray, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ray, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (lv_obj_t *cloud_part : screensaver_weather_cloud_parts) {
        if (!cloud_part) {
            continue;
        }
        if (show_cloud) {
            lv_obj_clear_flag(cloud_part, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(cloud_part, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (lv_obj_t *rain_line : screensaver_weather_rain_lines) {
        if (!rain_line) {
            continue;
        }
        if (show_rain) {
            lv_obj_clear_flag(rain_line, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(rain_line, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void updateWeatherMonogram()
{
    if (!screensaver_weather_group || !screensaver_weather_temp_label) {
        return;
    }

    if (!weather_has_data) {
        lv_obj_add_flag(screensaver_weather_group, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    setScreensaverWeatherGlyph(glyphForWeatherCode(weather_code, weather_is_day));
    lv_label_set_text_fmt(screensaver_weather_temp_label, "%d°", weather_temperature_c);

    int hour = 0;
    int minute = 0;
    int second = 0;
    getScreensaverClockTime(hour, minute, second);

    const float hour_angle = ((hour % 12) + (minute / 60.0f)) * 30.0f - 90.0f;
    const float hour_angle_rad = hour_angle * static_cast<float>(PI) / 180.0f;
    const bool hour_hand_in_top_half = sinf(hour_angle_rad) < 0.0f;
    const lv_coord_t radial_midpoint_offset = lv_obj_get_height(screensaver_clock_face) / 4;

    lv_obj_clear_flag(screensaver_weather_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(
        screensaver_weather_group,
        LV_ALIGN_CENTER,
        0,
        hour_hand_in_top_half ? radial_midpoint_offset : -radial_midpoint_offset);
}

void applyTimezoneSetting(bool request_sync)
{
    setenv("TZ", selectedTimezoneRule(), 1);
    tzset();

    updateTimezoneButtonLabel();
    weather_refresh_pending = true;
    weather_last_request_ms = 0;

    if (request_sync) {
        timezone_sync_pending = true;
    }

    if (WiFi.status() == WL_CONNECTED && timezone_sync_pending) {
        configTzTime(selectedTimezoneRule(), "pool.ntp.org", "time.nist.gov");
        timezone_sync_pending = false;
    }

    updateTimezoneStatusLabel();
}

void openTimezoneModal()
{
    if (!ui_timezone_modal || !ui_timezone_roller) {
        return;
    }

    lv_roller_set_selected(ui_timezone_roller, static_cast<uint16_t>(timezone_index), LV_ANIM_OFF);
    lv_obj_move_foreground(ui_timezone_modal);
    lv_obj_clear_flag(ui_timezone_modal, LV_OBJ_FLAG_HIDDEN);
    lv_disp_trig_activity(nullptr);
}

void closeTimezoneModal()
{
    if (!ui_timezone_modal) {
        return;
    }

    lv_obj_add_flag(ui_timezone_modal, LV_OBJ_FLAG_HIDDEN);
}

void ensureTimeIsConfigured()
{
    static wl_status_t last_wifi_status = WL_DISCONNECTED;
    const wl_status_t wifi_status = WiFi.status();

    if (wifi_status == WL_CONNECTED && (timezone_sync_pending || last_wifi_status != WL_CONNECTED)) {
        configTzTime(selectedTimezoneRule(), "pool.ntp.org", "time.nist.gov");
        timezone_sync_pending = false;
    }

    last_wifi_status = wifi_status;
    updateTimezoneStatusLabel();
}

bool fetchCurrentWeather()
{
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    const WeatherLocation &location = selectedWeatherLocation();
    String url = "https://api.open-meteo.com/v1/forecast?latitude=";
    url += String(location.latitude, 4);
    url += "&longitude=";
    url += String(location.longitude, 4);
    url += "&current=temperature_2m,weather_code,is_day&temperature_unit=celsius&forecast_days=1";

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(4000);
    if (!http.begin(client, url)) {
        return false;
    }

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    const String payload = http.getString();
    http.end();

    const int current_index = payload.indexOf("\"current\":");
    if (current_index < 0) {
        return false;
    }

    const int object_start = payload.indexOf('{', current_index);
    const int object_end = payload.indexOf('}', object_start);
    if (object_start < 0 || object_end < 0 || object_end <= object_start) {
        return false;
    }

    const String current_json = payload.substring(object_start, object_end + 1);

    float temperature = 0.0f;
    float code_value = 0.0f;
    float day_value = 1.0f;
    if (!extractJsonNumberField(current_json, "temperature_2m", temperature) ||
        !extractJsonNumberField(current_json, "weather_code", code_value) ||
        !extractJsonNumberField(current_json, "is_day", day_value)) {
        return false;
    }

    weather_temperature_c = (temperature >= 0.0f) ? static_cast<int>(temperature + 0.5f) : static_cast<int>(temperature - 0.5f);
    weather_code = static_cast<int>(code_value + 0.5f);
    weather_is_day = (day_value >= 0.5f);
    weather_has_data = true;
    weather_last_success_ms = millis();
    weather_refresh_pending = false;
    updateWeatherMonogram();
    return true;
}

void updateWeatherIfNeeded()
{
    static wl_status_t last_wifi_status = WL_DISCONNECTED;
    const wl_status_t wifi_status = WiFi.status();

    if (wifi_status != WL_CONNECTED) {
        last_wifi_status = wifi_status;
        return;
    }

    if (last_wifi_status != WL_CONNECTED) {
        weather_refresh_pending = true;
        weather_last_request_ms = 0;
    }
    last_wifi_status = wifi_status;

    const unsigned long now = millis();
    const bool weather_stale = (weather_last_success_ms == 0) || (now - weather_last_success_ms >= kWeatherRefreshMs);
    const bool retry_due = (weather_last_request_ms == 0) || (now - weather_last_request_ms >= kWeatherRetryMs);

    if (!(weather_refresh_pending || weather_stale) || !retry_due) {
        return;
    }

    weather_last_request_ms = now;
    fetchCurrentWeather();
}

void setClockHandPoints(lv_point_t points[2], float angle_deg, lv_coord_t cx, lv_coord_t cy, lv_coord_t length, lv_coord_t tail)
{
    const float angle_rad = angle_deg * static_cast<float>(PI) / 180.0f;
    const float cos_value = cosf(angle_rad);
    const float sin_value = sinf(angle_rad);

    points[0].x = static_cast<lv_coord_t>(cx - static_cast<lv_coord_t>(cos_value * tail));
    points[0].y = static_cast<lv_coord_t>(cy - static_cast<lv_coord_t>(sin_value * tail));
    points[1].x = static_cast<lv_coord_t>(cx + static_cast<lv_coord_t>(cos_value * length));
    points[1].y = static_cast<lv_coord_t>(cy + static_cast<lv_coord_t>(sin_value * length));
}

void updateScreensaverClock(bool force)
{
    if (!screensaver_clock_face || !screensaver_hour_hand || !screensaver_minute_hand || !screensaver_second_hand) {
        return;
    }

    static int last_second = -1;

    int hour = 0;
    int minute = 0;
    int second = 0;
    getScreensaverClockTime(hour, minute, second);

    if (!force && second == last_second) {
        return;
    }
    last_second = second;

    const lv_coord_t face_size = lv_obj_get_width(screensaver_clock_face);
    const lv_coord_t cx = face_size / 2;
    const lv_coord_t cy = face_size / 2;
    const lv_coord_t hour_length = (face_size * 26) / 100;
    const lv_coord_t minute_length = (face_size * 38) / 100;
    const lv_coord_t second_length = (face_size * 42) / 100;
    const lv_coord_t hour_tail = (face_size * 4) / 100;
    const lv_coord_t minute_tail = (face_size * 5) / 100;
    const lv_coord_t second_tail = (face_size * 7) / 100;

    const float hour_angle = ((hour % 12) + (minute / 60.0f)) * 30.0f - 90.0f;
    const float minute_angle = (minute + (second / 60.0f)) * 6.0f - 90.0f;
    const float second_angle = second * 6.0f - 90.0f;

    setClockHandPoints(screensaver_hour_points, hour_angle, cx, cy, hour_length, hour_tail);
    setClockHandPoints(screensaver_minute_points, minute_angle, cx, cy, minute_length, minute_tail);
    setClockHandPoints(screensaver_second_points, second_angle, cx, cy, second_length, second_tail);

    lv_line_set_points(screensaver_hour_hand, screensaver_hour_points, 2);
    lv_line_set_points(screensaver_minute_hand, screensaver_minute_points, 2);
    lv_line_set_points(screensaver_second_hand, screensaver_second_points, 2);
    updateWeatherMonogram();
}

void showScreensaver()
{
    if (!screensaver_overlay || screensaver_visible || !isClockSaverEnabled()) {
        return;
    }

    closeTimezoneModal();

    if (screen2_timer) {
        lv_timer_del(screen2_timer);
        screen2_timer = nullptr;
    }

    screensaver_visible = true;
    lv_obj_move_foreground(screensaver_overlay);
    lv_obj_clear_flag(screensaver_overlay, LV_OBJ_FLAG_HIDDEN);
    updateScrollbarModes();
    updateScreensaverClock(true);
}

void hideScreensaver(bool user_wake)
{
    if (!screensaver_overlay || !screensaver_visible) {
        return;
    }

    screensaver_visible = false;
    lv_obj_add_flag(screensaver_overlay, LV_OBJ_FLAG_HIDDEN);
    updateScrollbarModes();

    if (user_wake) {
        safeSetBrightness(global_brightness);
        syncScreen2IdleTimer();
    }
}

void updateOledProtection()
{
    static unsigned long last_shift_update = 0;
    static unsigned long last_clock_update = 0;
    static size_t shift_index = 0;

    if (!supportsPixelShift() && !supportsClockSaver()) {
        if (screensaver_visible) {
            hideScreensaver(false);
        }
        applyPixelShiftOffset(0, 0);
        return;
    }

    const unsigned long now = millis();
    const uint32_t inactive_time = lv_disp_get_inactive_time(nullptr);

    if (screensaver_visible && inactive_time <= kClockWakeDetectMs) {
        hideScreensaver(true);
        return;
    }

    if (isClockSaverEnabled()) {
        if (!screensaver_visible && inactive_time >= kClockSaverIdleMs) {
            showScreensaver();
            last_clock_update = now;
        }
    } else if (screensaver_visible) {
        hideScreensaver(false);
    }

    if (screensaver_visible && (last_clock_update == 0 || now - last_clock_update >= 1000UL)) {
        updateScreensaverClock(false);
        last_clock_update = now;
    }

    if (isPixelShiftEnabled()) {
        if (last_shift_update == 0) {
            shift_index = 0;
            applyPixelShiftOffset(0, 0);
            last_shift_update = now;
        } else if (now - last_shift_update >= kPixelShiftIntervalMs) {
            shift_index = (shift_index + 1U) % (sizeof(kPixelShiftSteps) / sizeof(kPixelShiftSteps[0]));
            applyPixelShiftOffset(
                static_cast<int>(kPixelShiftSteps[shift_index][0]) * kPixelShiftAmplitude,
                static_cast<int>(kPixelShiftSteps[shift_index][1]) * kPixelShiftAmplitude);
            last_shift_update = now;
        }
    } else {
        shift_index = 0;
        last_shift_update = 0;
        applyPixelShiftOffset(0, 0);
    }
}

void screen2_off_timer_cb(lv_timer_t *)
{
    lv_obj_t *active_tile = lv_tileview_get_tile_act(tileview);
    if (!screen2_bl_always_on && active_tile == tileMaster) {
        safeSetBrightness(0);
    }
    screen2_timer = nullptr;
}

void updateWiFiSymbol()
{
    if (!ui_wifi_label) {
        return;
    }

    const wl_status_t wifiStatus = WiFi.status();
    const bool isAPModeActive = (wifiStatus == WL_IDLE_STATUS || wifiStatus == WL_NO_SSID_AVAIL || wifiStatus == WL_NO_SHIELD);
    static wl_status_t lastStatus = WL_DISCONNECTED;

    if (wifiStatus != lastStatus) {
        if (wifiStatus == WL_CONNECTED) {
            lv_label_set_text(ui_wifi_label, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(ui_wifi_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else if (isAPModeActive) {
            lv_label_set_text(ui_wifi_label, LV_SYMBOL_SETTINGS);
            lv_obj_set_style_text_color(ui_wifi_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
        } else {
            lv_label_set_text(ui_wifi_label, LV_SYMBOL_WARNING);
            lv_obj_set_style_text_color(ui_wifi_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
        lastStatus = wifiStatus;
    }

    static bool switches_hidden = false;
    const lv_obj_t *current_tile = lv_tileview_get_tile_act(tileview);

    if (current_tile == tileMain || current_tile == tileSecond) {
        if (isAPModeActive && !switches_hidden) {
            lv_obj_clear_flag(ui_wifi_setup_msg, LV_OBJ_FLAG_HIDDEN);
            for (int i = 0; i < kNumUiSwitches; ++i) {
                lv_obj_add_flag(ui_switch_buttons[i], LV_OBJ_FLAG_HIDDEN);
            }
            switches_hidden = true;
        } else if (wifiStatus == WL_CONNECTED && switches_hidden) {
            lv_obj_add_flag(ui_wifi_setup_msg, LV_OBJ_FLAG_HIDDEN);
            for (int i = 0; i < kNumUiSwitches; ++i) {
                lv_obj_clear_flag(ui_switch_buttons[i], LV_OBJ_FLAG_HIDDEN);
            }
            switches_hidden = false;
        }
    }
}

void updateLVGLState(uint8_t id, bool state)
{
    if (id < kNumUiSwitches) {
        if (!ui_switch_buttons[id]) {
            return;
        }
        if (state) {
            lv_obj_add_state(ui_switch_buttons[id], LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ui_switch_buttons[id], LV_STATE_CHECKED);
        }
        return;
    }

    if (id == 8 && ui_big_button) {
        if (state) {
            lv_obj_add_state(ui_big_button, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ui_big_button, LV_STATE_CHECKED);
        }
    }
}

void applyNewColor(lv_color_t new_color)
{
    active_switch_color = new_color;

    for (int i = 0; i < kNumUiSwitches; ++i) {
        if (ui_switch_buttons[i]) {
            lv_obj_set_style_bg_color(ui_switch_buttons[i], active_switch_color, LV_STATE_CHECKED);
        }
    }

    if (ui_big_button) {
        lv_obj_set_style_bg_color(ui_big_button, active_switch_color, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(ui_big_button, active_switch_color, LV_STATE_CHECKED | LV_STATE_PRESSED);
    }

    if (ui_timezone_value_label) {
        lv_obj_set_style_text_color(ui_timezone_value_label, active_switch_color, 0);
    }
    if (ui_timezone_roller) {
        lv_obj_set_style_bg_color(ui_timezone_roller, active_switch_color, LV_PART_SELECTED);
        lv_obj_set_style_border_color(ui_timezone_roller, active_switch_color, LV_PART_SELECTED);
    }
    if (ui_timezone_done_button) {
        lv_obj_set_style_bg_color(ui_timezone_done_button, active_switch_color, 0);
    }

    applyClockAccentColor(active_switch_color);
}

void saveSettingsToNVS()
{
    preferences.begin("homespan", false);
    preferences.putInt("brightness", global_brightness);
    preferences.putUInt("color_hex", active_switch_color_hex);
    preferences.putBool("px_shift", oled_pixel_shift_enabled);
    preferences.putBool("clock_sv", oled_clock_saver_enabled);
    preferences.putInt("tz_idx", timezone_index);
    preferences.end();
}

void loadSettingsFromNVS()
{
    preferences.begin("homespan", true);
    global_brightness = preferences.getInt("brightness", 12);
    active_switch_color_hex = preferences.getUInt("color_hex", 0x68724D);
    oled_pixel_shift_enabled =
        preferences.getBool("px_shift", boardProfile().display_backend != DisplayBackendKind::TrgbPanel && supportsPixelShift());
    oled_clock_saver_enabled =
        preferences.getBool("clock_sv", boardProfile().display_backend != DisplayBackendKind::TrgbPanel && supportsClockSaver());
    timezone_index = preferences.getInt("tz_idx", kDefaultTimezoneIndex);
    preferences.end();

    if (timezone_index < 0 || timezone_index >= kNumTimezones) {
        timezone_index = kDefaultTimezoneIndex;
    }
    timezone_sync_pending = true;

    if (!supportsPixelShift()) {
        oled_pixel_shift_enabled = false;
    }
    if (!supportsClockSaver()) {
        oled_clock_saver_enabled = false;
    }

    active_switch_color = lv_color_hex(active_switch_color_hex);
}

void small_switch_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    auto *id_ptr = static_cast<uint8_t *>(lv_event_get_user_data(e));
    if (!id_ptr) {
        return;
    }

    const uint8_t id = *id_ptr;
    const bool newState = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    if (homekit_switches[id]) {
        homekit_switches[id]->on->setVal(newState);
    }
}

void big_switch_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *btn = lv_event_get_target(e);
    const bool newState = lv_obj_has_state(btn, LV_STATE_CHECKED);

    if (homekit_switches[8]) {
        homekit_switches[8]->on->setVal(newState);
    }

    safeSetBrightness(global_brightness);
    syncScreen2IdleTimer();
}

void brightness_slider_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    global_brightness = static_cast<int>(lv_slider_get_value(lv_event_get_target(e)));
    safeSetBrightness(global_brightness);
    saveSettingsToNVS();
}

void color_button_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    auto *id_ptr = static_cast<uint8_t *>(lv_event_get_user_data(e));
    if (!id_ptr) {
        return;
    }

    const uint8_t id = *id_ptr;
    const uint32_t new_color_hex = kColorHexOptions[id];

    applyNewColor(lv_color_hex(new_color_hex));
    active_switch_color_hex = new_color_hex;
    saveSettingsToNVS();
}

void bl_toggle_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    screen2_bl_always_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    syncScreen2IdleTimer();
}

void oled_pixel_shift_toggle_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    oled_pixel_shift_enabled = supportsPixelShift() && lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    if (!oled_pixel_shift_enabled) {
        applyPixelShiftOffset(0, 0);
    }
    saveSettingsToNVS();
}

void oled_clock_saver_toggle_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    oled_clock_saver_enabled = supportsClockSaver() && lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    if (!oled_clock_saver_enabled) {
        hideScreensaver(false);
    }
    saveSettingsToNVS();
}

void timezone_button_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    openTimezoneModal();
}

void timezone_roller_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    timezone_index = static_cast<int>(lv_roller_get_selected(lv_event_get_target(e)));
    applyTimezoneSetting(true);
    if (screensaver_visible) {
        updateScreensaverClock(true);
    }
}

void timezone_done_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    saveSettingsToNVS();
    closeTimezoneModal();
}

void screensaver_overlay_event_handler(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED) {
        return;
    }

    lv_event_stop_bubbling(e);
    lv_event_stop_processing(e);
    lv_disp_trig_activity(nullptr);
    hideScreensaver(true);
}

void tileview_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    updateScrollbarModes();
}

String generateAndStoreBridgeSuffix()
{
    const long random_val = random(0, 65536);
    char buffer[5];
    snprintf(buffer, sizeof(buffer), "%04X", static_cast<unsigned int>(random_val));

    preferences.begin("homespan", false);
    preferences.putString("br_suffix", buffer);
    preferences.end();
    return String(buffer);
}

void loadOrCreateBridgeSuffix()
{
    preferences.begin("homespan", false);
    const String suffix = preferences.getString("br_suffix", "");
    if (suffix.length() == 4) {
        BRIDGE_SUFFIX_STR = suffix;
    } else {
        BRIDGE_SUFFIX_STR = generateAndStoreBridgeSuffix();
    }
    preferences.end();
}

String generateAndStorePairingCode()
{
    String code;
    for (int i = 0; i < 8; ++i) {
        code += String(random(0, 10));
    }

    preferences.begin("homespan", false);
    preferences.putString("hk_code", code);
    preferences.end();
    return code;
}

void loadOrCreatePairingCode()
{
    preferences.begin("homespan", false);
    const String code = preferences.getString("hk_code", "");
    if (code.length() == 8) {
        HOMEKIT_PAIRING_CODE_STR = code;
    } else {
        HOMEKIT_PAIRING_CODE_STR = generateAndStorePairingCode();
    }
    preferences.end();
}

lv_obj_t *createSwitchButton(lv_obj_t *parent, uint8_t id, lv_coord_t x, lv_coord_t y, lv_coord_t size)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_make(50, 50, 50), 0);
    lv_obj_set_style_bg_color(btn, active_switch_color, LV_STATE_CHECKED);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(btn, small_switch_event_handler, LV_EVENT_CLICKED, &switch_button_ids[id]);
    return btn;
}

void setupUI()
{
    const UiMetrics &metrics = uiMetrics();
    const lv_coord_t full_size = metrics.canvas_size;
    const lv_coord_t center = metrics.center();
    const lv_coord_t root_x = metrics.canvas_x;
    const lv_coord_t root_y = metrics.canvas_y;
    const lv_coord_t safe_crop = static_cast<lv_coord_t>(pixelShiftSafeCropInset());
    const lv_coord_t viewport_size = full_size - (safe_crop * 2);
    const lv_coord_t compact_width = full_size - scaleUi(72);
    const lv_coord_t compact_row_height = scaleUi(62);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_size(scr, metrics.display_width, metrics.display_height);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

    ui_root = lv_obj_create(scr);
    lv_obj_remove_style_all(ui_root);
    lv_obj_set_size(ui_root, full_size, full_size);
    lv_obj_set_pos(ui_root, root_x, root_y);
    lv_obj_set_style_bg_color(ui_root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui_root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_root, LV_OBJ_FLAG_SCROLLABLE);

    ui_viewport = lv_obj_create(ui_root);
    lv_obj_remove_style_all(ui_viewport);
    lv_obj_set_size(ui_viewport, viewport_size, viewport_size);
    lv_obj_set_pos(ui_viewport, safe_crop, safe_crop);
    lv_obj_set_style_bg_color(ui_viewport, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui_viewport, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_viewport, LV_OBJ_FLAG_SCROLLABLE);

    ui_shift_layer = lv_obj_create(ui_viewport);
    lv_obj_remove_style_all(ui_shift_layer);
    lv_obj_set_size(ui_shift_layer, full_size, full_size);
    lv_obj_set_pos(ui_shift_layer, -safe_crop, -safe_crop);
    lv_obj_set_style_bg_color(ui_shift_layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui_shift_layer, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_shift_layer, LV_OBJ_FLAG_SCROLLABLE);

    tileview = lv_tileview_create(ui_shift_layer);
    lv_obj_set_size(tileview, full_size, full_size);
    lv_obj_set_style_bg_color(tileview, lv_color_black(), 0);
    lv_obj_set_style_anim_time(tileview, 0, LV_PART_SCROLLBAR);
    lv_obj_clear_flag(tileview, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag(tileview, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(tileview, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_event_cb(tileview, tileview_event_handler, LV_EVENT_VALUE_CHANGED, nullptr);

    tileMain = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_LEFT | LV_DIR_BOTTOM | LV_DIR_RIGHT);
    tileSecond = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_RIGHT | LV_DIR_LEFT);
    tileMaster = lv_tileview_add_tile(tileview, 0, 1, LV_DIR_TOP | LV_DIR_BOTTOM);
    tileSett = lv_tileview_add_tile(tileview, 0, 2, LV_DIR_TOP | LV_DIR_BOTTOM | LV_DIR_RIGHT);
    tileDisplayCare = lv_tileview_add_tile(tileview, 1, 2, LV_DIR_LEFT);
    tileCode = lv_tileview_add_tile(tileview, 0, 3, LV_DIR_TOP);
    lv_obj_set_style_anim_time(tileMain, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_anim_time(tileSecond, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_anim_time(tileMaster, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_anim_time(tileSett, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_anim_time(tileDisplayCare, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_anim_time(tileCode, 0, LV_PART_SCROLLBAR);
    hideScrollbarVisuals(tileview);
    hideScrollbarVisuals(tileMain);
    hideScrollbarVisuals(tileSecond);
    hideScrollbarVisuals(tileMaster);
    hideScrollbarVisuals(tileSett);
    hideScrollbarVisuals(tileDisplayCare);
    hideScrollbarVisuals(tileCode);
    lv_obj_set_tile(tileview, tileMain, LV_ANIM_OFF);

    const float angles[] = {270.0f, 0.0f, 90.0f, 180.0f};
    const int radius = uiMetrics().scale(120);
    const int button_size = uiMetrics().scale(120);

    for (int i = 0; i < kNumUiSwitches; ++i) {
        switch_button_ids[i] = static_cast<uint8_t>(i);
    }
    for (int i = 0; i < kNumColors; ++i) {
        color_button_ids[i] = static_cast<uint8_t>(i);
    }

    for (int i = 0; i < 4; ++i) {
        const float rad = static_cast<float>(PI) * angles[i] / 180.0f;
        const int x = center + static_cast<int>(radius * cosf(rad)) - (button_size / 2);
        const int y = center + static_cast<int>(radius * sinf(rad)) - (button_size / 2);
        ui_switch_buttons[i] = createSwitchButton(tileMain, static_cast<uint8_t>(i), x, y, button_size);
    }

    for (int i = 0; i < 4; ++i) {
        const float rad = static_cast<float>(PI) * angles[i] / 180.0f;
        const int x = center + static_cast<int>(radius * cosf(rad)) - (button_size / 2);
        const int y = center + static_cast<int>(radius * sinf(rad)) - (button_size / 2);
        ui_switch_buttons[i + 4] = createSwitchButton(tileSecond, static_cast<uint8_t>(i + 4), x, y, button_size);
    }

    ui_wifi_setup_msg = lv_label_create(tileMain);
    lv_label_set_text(
        ui_wifi_setup_msg,
        "Please connect to WLAN HOMEsmthng.\n"
        "If you have already set up the WLAN,\n"
        "please wait a moment or check the reception.\n"
        "\n"
        "(Note: screen updates calm down after\n"
        "the Wi-Fi connection is established.)");
    lv_obj_set_width(ui_wifi_setup_msg, full_size);
    lv_obj_align(ui_wifi_setup_msg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_align(ui_wifi_setup_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_wifi_setup_msg, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ui_wifi_setup_msg, lv_color_white(), 0);
    lv_obj_add_flag(ui_wifi_setup_msg, LV_OBJ_FLAG_HIDDEN);

    ui_big_button = lv_btn_create(tileMaster);
    lv_obj_set_size(ui_big_button, full_size, full_size);
    lv_obj_center(ui_big_button);
    lv_obj_set_style_radius(ui_big_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui_big_button, lv_color_black(), 0);
    lv_obj_set_style_bg_color(ui_big_button, active_switch_color, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui_big_button, active_switch_color, LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_add_flag(ui_big_button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_clear_flag(ui_big_button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(ui_big_button, big_switch_event_handler, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl_set = lv_label_create(tileSett);
    lv_label_set_text(lbl_set, "Settings");
    lv_obj_align(lbl_set, LV_ALIGN_TOP_MID, 0, scaleUi(20));
    lv_obj_set_style_text_font(lbl_set, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_set, lv_color_white(), 0);

    lv_obj_t *lbl_color = lv_label_create(tileSett);
    lv_label_set_text(lbl_color, "Switch ON Color");
    lv_obj_set_style_text_color(lbl_color, lv_color_white(), 0);
    lv_obj_align_to(lbl_color, lbl_set, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(15));

    lv_obj_t *color_panel = lv_obj_create(tileSett);
    lv_obj_set_size(color_panel, full_size - scaleUi(40), scaleUi(60));
    lv_obj_align_to(color_panel, lbl_color, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(5));
    lv_obj_set_style_bg_color(color_panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(color_panel, 0, 0);
    lv_obj_set_style_pad_all(color_panel, 0, 0);
    lv_obj_set_style_pad_gap(color_panel, scaleUi(10), 0);
    hideScrollbarVisuals(color_panel);
    lv_obj_set_flex_flow(color_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(color_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < kNumColors; ++i) {
        lv_obj_t *color_btn = lv_btn_create(color_panel);
        lv_obj_set_size(color_btn, scaleUi(40), scaleUi(40));
        lv_obj_set_style_bg_color(color_btn, lv_color_hex(kColorHexOptions[i]), 0);
        lv_obj_set_style_radius(color_btn, scaleUi(5), 0);
        lv_obj_add_event_cb(color_btn, color_button_event_handler, LV_EVENT_CLICKED, &color_button_ids[i]);
    }

    lv_obj_t *lbl_bri_title = lv_label_create(tileSett);
    lv_label_set_text(lbl_bri_title, "Display Brightness");
    lv_obj_set_style_text_color(lbl_bri_title, lv_color_white(), 0);
    lv_obj_align_to(lbl_bri_title, color_panel, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(30));

    ui_brightness_slider = lv_slider_create(tileSett);
    lv_obj_set_width(ui_brightness_slider, scaleUi(300));
    lv_obj_align_to(ui_brightness_slider, lbl_bri_title, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(5));
    lv_slider_set_range(ui_brightness_slider, 1, boardProfile().brightness_levels);
    lv_slider_set_value(ui_brightness_slider, global_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *bl_cont = lv_obj_create(tileSett);
    lv_obj_set_size(bl_cont, full_size - scaleUi(40), scaleUi(75));
    lv_obj_align_to(bl_cont, ui_brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(20));
    lv_obj_set_style_bg_color(bl_cont, lv_color_black(), 0);
    lv_obj_set_style_border_width(bl_cont, 0, 0);
    lv_obj_set_style_pad_all(bl_cont, 0, 0);
    hideScrollbarVisuals(bl_cont);
    lv_obj_set_style_layout(bl_cont, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(bl_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bl_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *bl_lbl = lv_label_create(bl_cont);
    lv_label_set_text(
        bl_lbl,
        "Keep Screen 2 backlight ON\n"
        "(Disable if you want to use the display\n"
        "as a switch without it lighting up)");
    lv_obj_set_style_text_color(bl_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(bl_lbl, &lv_font_montserrat_16, 0);

    ui_bl_toggle = lv_switch_create(bl_cont);
    if (screen2_bl_always_on) {
        lv_obj_add_state(ui_bl_toggle, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(ui_bl_toggle, bl_toggle_event_handler, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *lbl_home_status = lv_label_create(tileSett);
    lv_label_set_text(lbl_home_status, "HOMEsmthng");
    lv_obj_set_style_text_font(lbl_home_status, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_home_status, lv_color_white(), 0);
    lv_obj_align_to(lbl_home_status, bl_cont, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(20));

    lv_obj_t *lbl_subtitle = lv_label_create(tileSett);
    lv_label_set_text(lbl_subtitle, "developed with HomeSpan");
    lv_obj_set_style_text_font(lbl_subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_subtitle, lv_color_make(180, 180, 180), 0);
    lv_obj_align_to(lbl_subtitle, lbl_home_status, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(5));

    lv_obj_t *lbl_swipe_hint_sett = lv_label_create(tileSett);
    lv_label_set_text(lbl_swipe_hint_sett, LV_SYMBOL_UP "   " LV_SYMBOL_DOWN "   " LV_SYMBOL_RIGHT);
    lv_obj_align(lbl_swipe_hint_sett, LV_ALIGN_BOTTOM_MID, 0, -scaleUi(12));
    lv_obj_set_style_text_font(lbl_swipe_hint_sett, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_swipe_hint_sett, lv_color_make(180, 180, 180), 0);

    lv_obj_t *lbl_care_title = lv_label_create(tileDisplayCare);
    lv_label_set_text(lbl_care_title, "Display Care");
    lv_obj_align(lbl_care_title, LV_ALIGN_TOP_MID, 0, scaleUi(20));
    lv_obj_set_style_text_font(lbl_care_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_care_title, lv_color_white(), 0);

    lv_obj_t *pixel_shift_row = lv_obj_create(tileDisplayCare);
    lv_obj_remove_style_all(pixel_shift_row);
    lv_obj_set_size(pixel_shift_row, compact_width, compact_row_height);
    lv_obj_align_to(pixel_shift_row, lbl_care_title, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(42));
    lv_obj_set_style_pad_gap(pixel_shift_row, scaleUi(10), 0);
    hideScrollbarVisuals(pixel_shift_row);
    lv_obj_clear_flag(pixel_shift_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_layout(pixel_shift_row, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(pixel_shift_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pixel_shift_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *pixel_shift_label = lv_label_create(pixel_shift_row);
    lv_label_set_text(pixel_shift_label, "Pixel Shift");
    lv_obj_set_width(pixel_shift_label, compact_width - scaleUi(96));
    lv_label_set_long_mode(pixel_shift_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(pixel_shift_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(pixel_shift_label, lv_color_white(), 0);

    ui_pixel_shift_toggle = lv_switch_create(pixel_shift_row);
    if (oled_pixel_shift_enabled) {
        lv_obj_add_state(ui_pixel_shift_toggle, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(ui_pixel_shift_toggle, oled_pixel_shift_toggle_event_handler, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *clock_saver_row = lv_obj_create(tileDisplayCare);
    lv_obj_remove_style_all(clock_saver_row);
    lv_obj_set_size(clock_saver_row, compact_width, compact_row_height);
    lv_obj_align_to(clock_saver_row, pixel_shift_row, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(16));
    lv_obj_set_style_pad_gap(clock_saver_row, scaleUi(10), 0);
    hideScrollbarVisuals(clock_saver_row);
    lv_obj_clear_flag(clock_saver_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_layout(clock_saver_row, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(clock_saver_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clock_saver_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *clock_saver_label = lv_label_create(clock_saver_row);
    lv_label_set_text(clock_saver_label, "Clock Saver");
    lv_obj_set_width(clock_saver_label, compact_width - scaleUi(96));
    lv_label_set_long_mode(clock_saver_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(clock_saver_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(clock_saver_label, lv_color_white(), 0);

    ui_clock_saver_toggle = lv_switch_create(clock_saver_row);
    if (oled_clock_saver_enabled) {
        lv_obj_add_state(ui_clock_saver_toggle, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(ui_clock_saver_toggle, oled_clock_saver_toggle_event_handler, LV_EVENT_VALUE_CHANGED, nullptr);

    ui_timezone_button = lv_obj_create(tileDisplayCare);
    lv_obj_remove_style_all(ui_timezone_button);
    lv_obj_set_size(ui_timezone_button, compact_width, compact_row_height);
    lv_obj_align_to(ui_timezone_button, clock_saver_row, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(16));
    lv_obj_set_style_pad_gap(ui_timezone_button, scaleUi(10), 0);
    hideScrollbarVisuals(ui_timezone_button);
    lv_obj_add_flag(ui_timezone_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_timezone_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_layout(ui_timezone_button, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(ui_timezone_button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_timezone_button, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(ui_timezone_button, timezone_button_event_handler, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *timezone_label = lv_label_create(ui_timezone_button);
    lv_label_set_text(timezone_label, "Timezone");
    lv_obj_set_style_text_font(timezone_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(timezone_label, lv_color_white(), 0);

    ui_timezone_value_label = lv_label_create(ui_timezone_button);
    lv_obj_set_width(ui_timezone_value_label, compact_width - scaleUi(150));
    lv_label_set_long_mode(ui_timezone_value_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(ui_timezone_value_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(ui_timezone_value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui_timezone_value_label, active_switch_color, 0);

    ui_timezone_status_label = lv_label_create(tileDisplayCare);
    lv_obj_set_width(ui_timezone_status_label, compact_width);
    lv_label_set_long_mode(ui_timezone_status_label, LV_LABEL_LONG_CLIP);
    lv_obj_align_to(ui_timezone_status_label, ui_timezone_button, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(24));
    lv_obj_set_style_text_align(ui_timezone_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_timezone_status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui_timezone_status_label, lv_color_make(190, 190, 190), 0);

    lv_obj_t *lbl_swipe_hint_left = lv_label_create(tileDisplayCare);
    lv_label_set_text(lbl_swipe_hint_left, LV_SYMBOL_LEFT);
    lv_obj_align(lbl_swipe_hint_left, LV_ALIGN_BOTTOM_MID, 0, -scaleUi(12));
    lv_obj_set_style_text_font(lbl_swipe_hint_left, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_swipe_hint_left, lv_color_make(180, 180, 180), 0);

    const String formatted_code = HOMEKIT_PAIRING_CODE_STR.substring(0, 4) + "-" + HOMEKIT_PAIRING_CODE_STR.substring(4, 8);

    lv_obj_t *lbl_pairing_title = lv_label_create(tileCode);
    lv_label_set_text(lbl_pairing_title, "HomeKit Pairing Code");
    lv_obj_set_width(lbl_pairing_title, full_size);
    lv_obj_align(lbl_pairing_title, LV_ALIGN_TOP_MID, 0, scaleUi(80));
    lv_obj_set_style_text_align(lbl_pairing_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_pairing_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_pairing_title, lv_color_white(), 0);

    lv_obj_t *lbl_pairing_code = lv_label_create(tileCode);
    lv_label_set_text(lbl_pairing_code, formatted_code.c_str());
    lv_obj_set_width(lbl_pairing_code, full_size);
    lv_obj_align_to(lbl_pairing_code, lbl_pairing_title, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(20));
    lv_obj_set_style_text_align(lbl_pairing_code, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_pairing_code, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_pairing_code, lv_color_white(), 0);

    lv_obj_t *lbl_fallback = lv_label_create(tileCode);
    lv_label_set_text(
        lbl_fallback,
        "If pairing code doesn't work,\n"
        "please try: 2244-6688, 4663-7726 or 1234-5678\n"
        "(Try to be as close to your router as possible,\n"
        "as this speeds up the process considerably and\n"
        "also prevents the pairing process from failing)");
    lv_obj_set_width(lbl_fallback, full_size);
    lv_obj_align_to(lbl_fallback, lbl_pairing_code, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(15));
    lv_obj_set_style_text_align(lbl_fallback, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_fallback, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_fallback, lv_color_make(180, 180, 180), 0);

    ui_wifi_label = lv_label_create(tileCode);
    lv_label_set_text(ui_wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(ui_wifi_label, &lv_font_montserrat_48, 0);
    lv_obj_align(ui_wifi_label, LV_ALIGN_BOTTOM_MID, 0, -scaleUi(116));
    lv_obj_set_style_text_color(ui_wifi_label, lv_color_white(), 0);

    lv_obj_t *lbl_back_to_settings = lv_label_create(tileCode);
    lv_label_set_text(lbl_back_to_settings, "Back to Settings");
    lv_obj_align_to(lbl_back_to_settings, ui_wifi_label, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(6));
    lv_obj_set_style_text_font(lbl_back_to_settings, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_back_to_settings, lv_color_make(180, 180, 180), 0);

    lv_obj_t *lbl_swipe_hint_up = lv_label_create(tileCode);
    lv_label_set_text(lbl_swipe_hint_up, LV_SYMBOL_UP);
    lv_obj_align_to(lbl_swipe_hint_up, lbl_back_to_settings, LV_ALIGN_OUT_BOTTOM_MID, 0, scaleUi(6));
    lv_obj_set_style_text_font(lbl_swipe_hint_up, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_swipe_hint_up, lv_color_make(180, 180, 180), 0);

    ui_timezone_modal = lv_obj_create(scr);
    lv_obj_remove_style_all(ui_timezone_modal);
    lv_obj_set_size(ui_timezone_modal, metrics.display_width, metrics.display_height);
    lv_obj_set_style_bg_color(ui_timezone_modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui_timezone_modal, LV_OPA_90, 0);
    hideScrollbarVisuals(ui_timezone_modal);
    lv_obj_add_flag(ui_timezone_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_timezone_modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_timezone_modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *timezone_modal_panel = lv_obj_create(ui_timezone_modal);
    lv_obj_remove_style_all(timezone_modal_panel);
    lv_obj_set_size(timezone_modal_panel, compact_width, full_size - scaleUi(110));
    lv_obj_center(timezone_modal_panel);
    hideScrollbarVisuals(timezone_modal_panel);
    lv_obj_clear_flag(timezone_modal_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *timezone_modal_title = lv_label_create(timezone_modal_panel);
    lv_label_set_text(timezone_modal_title, "Timezone");
    lv_obj_align(timezone_modal_title, LV_ALIGN_TOP_MID, 0, scaleUi(6));
    lv_obj_set_style_text_font(timezone_modal_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(timezone_modal_title, lv_color_white(), 0);

    ui_timezone_roller = lv_roller_create(timezone_modal_panel);
    lv_roller_set_options(ui_timezone_roller, kTimezoneDropdownOptions, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(ui_timezone_roller, kTimezoneVisibleRows);
    lv_roller_set_selected(ui_timezone_roller, static_cast<uint16_t>(timezone_index), LV_ANIM_OFF);
    lv_obj_set_width(ui_timezone_roller, compact_width - scaleUi(24));
    lv_obj_align(ui_timezone_roller, LV_ALIGN_CENTER, 0, -scaleUi(8));
    lv_obj_set_style_bg_color(ui_timezone_roller, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui_timezone_roller, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_timezone_roller, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_timezone_roller, &lv_font_montserrat_24, 0);
    lv_obj_set_style_bg_color(ui_timezone_roller, active_switch_color, LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(ui_timezone_roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(ui_timezone_roller, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_border_width(ui_timezone_roller, 0, 0);
    lv_obj_set_style_border_width(ui_timezone_roller, 0, LV_PART_SELECTED);
    hideScrollbarVisuals(ui_timezone_roller);
    lv_obj_add_event_cb(ui_timezone_roller, timezone_roller_event_handler, LV_EVENT_VALUE_CHANGED, nullptr);

    ui_timezone_done_button = lv_btn_create(timezone_modal_panel);
    lv_obj_set_size(ui_timezone_done_button, scaleUi(150), scaleUi(46));
    lv_obj_align(ui_timezone_done_button, LV_ALIGN_BOTTOM_MID, 0, -scaleUi(12));
    lv_obj_set_style_radius(ui_timezone_done_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ui_timezone_done_button, 0, 0);
    lv_obj_set_style_bg_color(ui_timezone_done_button, active_switch_color, 0);
    lv_obj_add_event_cb(ui_timezone_done_button, timezone_done_event_handler, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *timezone_done_label = lv_label_create(ui_timezone_done_button);
    lv_label_set_text(timezone_done_label, "Done");
    lv_obj_center(timezone_done_label);
    lv_obj_set_style_text_color(timezone_done_label, lv_color_white(), 0);

    updateTimezoneButtonLabel();
    updateTimezoneStatusLabel();

    screensaver_overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(screensaver_overlay);
    lv_obj_set_size(screensaver_overlay, metrics.display_width, metrics.display_height);
    lv_obj_set_style_bg_color(screensaver_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screensaver_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_anim_time(screensaver_overlay, 0, LV_PART_SCROLLBAR);
    hideScrollbarVisuals(screensaver_overlay);
    lv_obj_add_flag(screensaver_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screensaver_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(screensaver_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screensaver_overlay, screensaver_overlay_event_handler, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(screensaver_overlay, screensaver_overlay_event_handler, LV_EVENT_RELEASED, nullptr);

    screensaver_clock_layer = lv_obj_create(screensaver_overlay);
    lv_obj_remove_style_all(screensaver_clock_layer);
    lv_obj_set_size(screensaver_clock_layer, metrics.display_width, metrics.display_height);
    lv_obj_set_pos(screensaver_clock_layer, 0, 0);
    lv_obj_set_style_anim_time(screensaver_clock_layer, 0, LV_PART_SCROLLBAR);
    hideScrollbarVisuals(screensaver_clock_layer);
    lv_obj_add_flag(screensaver_clock_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(screensaver_clock_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screensaver_clock_layer, screensaver_overlay_event_handler, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(screensaver_clock_layer, screensaver_overlay_event_handler, LV_EVENT_RELEASED, nullptr);

    screensaver_clock_face = lv_obj_create(screensaver_clock_layer);
    const lv_coord_t screensaver_face_size = full_size - scaleUi(8);
    lv_obj_set_size(screensaver_clock_face, screensaver_face_size, screensaver_face_size);
    lv_obj_center(screensaver_clock_face);
    lv_obj_set_style_radius(screensaver_clock_face, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(screensaver_clock_face, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screensaver_clock_face, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(screensaver_clock_face, 0, 0);
    lv_obj_set_style_pad_all(screensaver_clock_face, 0, 0);
    lv_obj_set_style_anim_time(screensaver_clock_face, 0, LV_PART_SCROLLBAR);
    hideScrollbarVisuals(screensaver_clock_face);
    lv_obj_add_flag(screensaver_clock_face, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(screensaver_clock_face, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screensaver_clock_face, screensaver_overlay_event_handler, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(screensaver_clock_face, screensaver_overlay_event_handler, LV_EVENT_RELEASED, nullptr);

    screensaver_hour_hand = lv_line_create(screensaver_clock_face);
    lv_obj_set_size(screensaver_hour_hand, screensaver_face_size, screensaver_face_size);
    lv_obj_set_pos(screensaver_hour_hand, 0, 0);
    lv_obj_set_style_line_width(screensaver_hour_hand, scaleUi(6), 0);
    lv_obj_set_style_line_color(screensaver_hour_hand, lv_color_make(230, 230, 230), 0);
    lv_obj_set_style_line_opa(screensaver_hour_hand, LV_OPA_90, 0);
    lv_obj_set_style_line_rounded(screensaver_hour_hand, true, 0);

    screensaver_minute_hand = lv_line_create(screensaver_clock_face);
    lv_obj_set_size(screensaver_minute_hand, screensaver_face_size, screensaver_face_size);
    lv_obj_set_pos(screensaver_minute_hand, 0, 0);
    lv_obj_set_style_line_width(screensaver_minute_hand, scaleUi(4), 0);
    lv_obj_set_style_line_color(screensaver_minute_hand, lv_color_make(180, 180, 180), 0);
    lv_obj_set_style_line_opa(screensaver_minute_hand, LV_OPA_80, 0);
    lv_obj_set_style_line_rounded(screensaver_minute_hand, true, 0);

    screensaver_second_hand = lv_line_create(screensaver_clock_face);
    lv_obj_set_size(screensaver_second_hand, screensaver_face_size, screensaver_face_size);
    lv_obj_set_pos(screensaver_second_hand, 0, 0);
    lv_obj_set_style_line_width(screensaver_second_hand, scaleUi(2), 0);
    lv_obj_set_style_line_color(screensaver_second_hand, lv_color_make(120, 120, 120), 0);
    lv_obj_set_style_line_opa(screensaver_second_hand, LV_OPA_70, 0);
    lv_obj_set_style_line_rounded(screensaver_second_hand, true, 0);

    screensaver_center_dot = lv_obj_create(screensaver_clock_face);
    lv_obj_remove_style_all(screensaver_center_dot);
    lv_obj_set_size(screensaver_center_dot, scaleUi(10), scaleUi(10));
    lv_obj_center(screensaver_center_dot);
    lv_obj_set_style_radius(screensaver_center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(screensaver_center_dot, LV_OPA_90, 0);

    screensaver_weather_group = lv_obj_create(screensaver_clock_face);
    lv_obj_remove_style_all(screensaver_weather_group);
    lv_obj_set_size(screensaver_weather_group, scaleUi(156), scaleUi(58));
    lv_obj_set_style_bg_opa(screensaver_weather_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_layout(screensaver_weather_group, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(screensaver_weather_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        screensaver_weather_group,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(screensaver_weather_group, 0, 0);
    lv_obj_set_style_pad_gap(screensaver_weather_group, scaleUi(10), 0);
    lv_obj_clear_flag(screensaver_weather_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screensaver_weather_group, LV_OBJ_FLAG_HIDDEN);
    hideScrollbarVisuals(screensaver_weather_group);

    screensaver_weather_icon = lv_obj_create(screensaver_weather_group);
    lv_obj_remove_style_all(screensaver_weather_icon);
    lv_obj_set_size(screensaver_weather_icon, scaleUi(42), scaleUi(42));
    lv_obj_set_style_bg_opa(screensaver_weather_icon, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(screensaver_weather_icon, LV_OBJ_FLAG_SCROLLABLE);

    screensaver_weather_sun_core = lv_obj_create(screensaver_weather_icon);
    lv_obj_remove_style_all(screensaver_weather_sun_core);
    lv_obj_set_size(screensaver_weather_sun_core, scaleUi(18), scaleUi(18));
    lv_obj_align(screensaver_weather_sun_core, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(screensaver_weather_sun_core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(screensaver_weather_sun_core, LV_OPA_COVER, 0);

    const lv_coord_t ray_lengths[4][4] = {
        {scaleUi(20), 0, scaleUi(2), scaleUi(8)},
        {scaleUi(20), scaleUi(34), scaleUi(2), scaleUi(8)},
        {0, scaleUi(20), scaleUi(8), scaleUi(2)},
        {scaleUi(34), scaleUi(20), scaleUi(8), scaleUi(2)},
    };
    for (int i = 0; i < 4; ++i) {
        screensaver_weather_sun_rays[i] = lv_obj_create(screensaver_weather_icon);
        lv_obj_remove_style_all(screensaver_weather_sun_rays[i]);
        lv_obj_set_size(screensaver_weather_sun_rays[i], ray_lengths[i][2], ray_lengths[i][3]);
        lv_obj_set_pos(screensaver_weather_sun_rays[i], ray_lengths[i][0], ray_lengths[i][1]);
        lv_obj_set_style_radius(screensaver_weather_sun_rays[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(screensaver_weather_sun_rays[i], LV_OPA_COVER, 0);
    }

    const lv_coord_t cloud_rects[4][4] = {
        {scaleUi(3), scaleUi(14), scaleUi(15), scaleUi(15)},
        {scaleUi(13), scaleUi(8), scaleUi(18), scaleUi(18)},
        {scaleUi(26), scaleUi(14), scaleUi(13), scaleUi(13)},
        {scaleUi(8), scaleUi(21), scaleUi(28), scaleUi(11)},
    };
    for (int i = 0; i < 4; ++i) {
        screensaver_weather_cloud_parts[i] = lv_obj_create(screensaver_weather_icon);
        lv_obj_remove_style_all(screensaver_weather_cloud_parts[i]);
        lv_obj_set_size(screensaver_weather_cloud_parts[i], cloud_rects[i][2], cloud_rects[i][3]);
        lv_obj_set_pos(screensaver_weather_cloud_parts[i], cloud_rects[i][0], cloud_rects[i][1]);
        lv_obj_set_style_radius(
            screensaver_weather_cloud_parts[i],
            (i == 3) ? scaleUi(6) : LV_RADIUS_CIRCLE,
            0);
        lv_obj_set_style_bg_opa(screensaver_weather_cloud_parts[i], LV_OPA_COVER, 0);
    }

    const lv_coord_t rain_rects[3][4] = {
        {scaleUi(11), scaleUi(31), scaleUi(2), scaleUi(8)},
        {scaleUi(20), scaleUi(33), scaleUi(2), scaleUi(8)},
        {scaleUi(29), scaleUi(31), scaleUi(2), scaleUi(8)},
    };
    for (int i = 0; i < 3; ++i) {
        screensaver_weather_rain_lines[i] = lv_obj_create(screensaver_weather_icon);
        lv_obj_remove_style_all(screensaver_weather_rain_lines[i]);
        lv_obj_set_size(screensaver_weather_rain_lines[i], rain_rects[i][2], rain_rects[i][3]);
        lv_obj_set_pos(screensaver_weather_rain_lines[i], rain_rects[i][0], rain_rects[i][1]);
        lv_obj_set_style_radius(screensaver_weather_rain_lines[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(screensaver_weather_rain_lines[i], LV_OPA_COVER, 0);
    }

    screensaver_weather_temp_label = lv_label_create(screensaver_weather_group);
    lv_label_set_text(screensaver_weather_temp_label, "--°");
    lv_obj_set_style_text_font(screensaver_weather_temp_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(screensaver_weather_temp_label, lv_color_white(), 0);

    applyClockAccentColor(active_switch_color);
    setScreensaverWeatherGlyph(weather_glyph);
    updateWeatherMonogram();
    updateScreensaverClock(true);
    updateScrollbarModes();
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.printf("HOMEsmthng booting on %s\n", boardProfile().name);

    loadSettingsFromNVS();
    loadOrCreatePairingCode();
    loadOrCreateBridgeSuffix();

    HOMEKIT_PAIRING_CODE = HOMEKIT_PAIRING_CODE_STR.c_str();
    homeSpan.setPairingCode(HOMEKIT_PAIRING_CODE);

    if (!displayBackend().begin()) {
        Serial.println("Display initialization failed");
        while (true) {
            delay(1000);
        }
    }

    setupUI();
    applyTimezoneSetting(true);

    const String final_bridge_name = String("HOMEsmthng ") + BRIDGE_SUFFIX_STR;
    homeSpan.begin(Category::Bridges, final_bridge_name.c_str());
    homeSpan.setApSSID("HOMEsmthng");
    homeSpan.setApPassword("");
    homeSpan.enableAutoStartAP();

    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Name(boardProfile().accessory_name);

    for (int i = 0; i < kNumSwitches; ++i) {
        new SpanAccessory();
        new Service::AccessoryInformation();
        new Characteristic::Identify();
        new Characteristic::Name(kSwitchNames[i]);
        homekit_switches[i] = new MySwitch(static_cast<uint8_t>(i));
    }

    for (int i = 0; i < kNumSwitches; ++i) {
        if (homekit_switches[i]) {
            updateLVGLState(static_cast<uint8_t>(i), homekit_switches[i]->on->getVal());
        }
    }

    delay(100);
    updateWiFiSymbol();
    updateTimezoneStatusLabel();
    applyPixelShiftOffset(0, 0);
    safeSetBrightness(global_brightness);
}

void loop()
{
    lv_timer_handler();
    homeSpan.poll();

    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1000) {
        lastUpdate = millis();
        updateWiFiSymbol();
        ensureTimeIsConfigured();
        updateWeatherIfNeeded();
    }

    updateOledProtection();

    lv_obj_t *active_tile = lv_tileview_get_tile_act(tileview);
    int target_brightness_level = global_brightness;

    if (screensaver_visible) {
        target_brightness_level = screensaverBrightness();
    } else if (active_tile == tileMaster && !screen2_bl_always_on && screen2_timer == nullptr) {
        target_brightness_level = 0;
    }

    safeSetBrightness(target_brightness_level);
    delay(5);
}
