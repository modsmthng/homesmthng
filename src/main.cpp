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
#include <WebServer.h>
#include <DNSServer.h>
#include <lvgl.h>
#include <nvs.h>
#include <qrcodegen.h>

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
constexpr char kHomeKitSetupId[] = "HSPN";
constexpr char kHomeSpanWifiNamespace[] = "WIFI";
constexpr char kHomeSpanWifiKey[] = "WIFIDATA";
constexpr char kDefaultHostBase[] = "homesmthng";

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
const uint32_t kColorHexOptions[kNumColors] = {0xFF0000, 0xFF9900, 0xF5F1E8, 0xA8C6FE, 0x68724D, 0xF99963};
const char *kColorLabels[kNumColors] = {"Red", "Orange", "Warm White", "Blue", "Olive", "Peach"};
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
bool screen2_bl_always_on = true;
bool oled_pixel_shift_enabled = false;
bool oled_clock_saver_enabled = true;
bool clock_button_screen_enabled = true;
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

void saveSettingsToNVS();
void updateLVGLState(uint8_t id, bool state);
void updateWiFiSymbol();
void safeSetBrightness(int target_brightness);
void applyNewColor(lv_color_t new_color);
void screen2_off_timer_cb(lv_timer_t *);
void applyClockAccentColor(lv_color_t color);
void updateMasterClockAppearance();
void hideScrollbarVisuals(lv_obj_t *obj);
void disableDecorationInteraction(lv_obj_t *obj);
void updateScrollbarModes();
bool supportsPixelShift();
bool supportsClockSaver();
bool isPixelShiftEnabled();
bool isClockSaverEnabled();
bool isClockButtonEnabled();
bool isWeatherEnabled();
bool shouldShowClockWeather();
int screensaverBrightness();
int pixelShiftSafeCropInset();
bool hasCustomWeatherLocation();
bool isValidWeatherCoordinates(float latitude, float longitude);
WeatherLocation selectedWeatherLocation();
String selectedWeatherLocationLabel();
void syncScreen2IdleTimer();
void applyPixelShiftOffset(int shift_x, int shift_y);
void setClockHandPoints(lv_point_t points[2], float angle_deg, lv_coord_t cx, lv_coord_t cy, lv_coord_t length, lv_coord_t tail);
void updateScreensaverClock(bool force);
void updateMasterClock(bool force);
void updateMasterClockMode();
void updateWeatherMonogram();
bool extractJsonNumberField(const String &json, const char *key, float &value);
bool extractJsonStringField(const String &json, const char *key, String &value);
bool parseFloatParameter(const String &text, float &value);
String urlEncode(const String &text);
WeatherGlyph glyphForWeatherCode(int code, bool is_day);
const char *weatherGlyphLabel(WeatherGlyph glyph);
void setScreensaverWeatherGlyph(WeatherGlyph glyph);
void setMasterClockWeatherGlyph(WeatherGlyph glyph);
bool fetchCurrentWeather();
int parseWeatherSearchResults(const String &json, WeatherSearchResult results[], int max_results);
bool fetchWeatherSearchResults(
    const String &query,
    WeatherSearchResult results[],
    int max_results,
    int &result_count,
    String &error_message);
void updateWeatherIfNeeded();
String htmlEscape(const String &text);
String weatherSearchDisplayName(const WeatherSearchResult &result);
String defaultDeviceHostName();
String defaultDeviceLabel();
String sanitizeHostName(const String &text);
String formattedPairingCode();
String setupAccessPointSsid();
String setupPortalUrl();
String adminMdnsUrl();
String adminDirectUrl();
bool isPairingOnboardingActive();
String defaultAdminSection(const String &requested_section);
bool readStoredWifiCredentials(StoredWifiCredentials &credentials);
bool saveStoredWifiCredentials(const StoredWifiCredentials &credentials);
void eraseStoredWifiCredentials();
void loadOrCreateDeviceConfig();
void saveDeviceConfigToNVS();
void applyBrightnessSetting(int brightness, bool persist);
int brightnessScaleForWeb(int brightness);
int brightnessLevelFromWebScale(int scale_value);
bool parseHexColor(const String &text, uint32_t &color_hex);
String colorHexCss(uint32_t color_hex);
void applyScreen2BacklightSetting(bool enabled, bool persist);
void applyPixelShiftSetting(bool enabled, bool persist);
void applyClockButtonSetting(bool enabled, bool persist);
void applyClockSaverSetting(bool enabled, bool persist);
void applyClockSaverIdleSetting(uint32_t idle_ms, bool persist);
void applyTimezoneSettingIndex(int index, bool persist);
void applyWeatherEnabledSetting(bool enabled, bool persist);
void saveCustomWeatherLocation(const String &name, float latitude, float longitude, bool persist);
void resetCustomWeatherLocation(bool persist);
void updateWifiSetupMessage();
void deferWeatherRefresh(uint32_t delay_ms);
String homeKitSetupPayload();
String renderQrSvg(const String &payload);
String renderWebHomePage();
String renderAdminPage(
    const String &message = "",
    bool is_error = false,
    const String &search_query = "",
    const WeatherSearchResult *search_results = nullptr,
    int search_result_count = 0,
    const String &search_message = "",
    bool search_is_error = false,
    const String &active_section = "");
String renderWeatherConfigPage(
    const String &message = "",
    const String &search_query = "",
    const WeatherSearchResult *search_results = nullptr,
    int search_result_count = 0,
    const String &search_message = "");
String renderProvisioningPage(const String &message = "", bool is_error = false);
void handleWebHome();
void handleWeatherConfigRoot();
void handleWeatherConfigSearch();
void handleWeatherConfigSave();
void handleWeatherConfigReset();
void redirectToWeatherSection();
void handleDisplaySettingsSave();
void handleTimezoneSave();
void handleDeviceConfigSave();
void handleWifiConfigSave();
void handleWifiReset();
void handleHomeKitQrSvg();
void startWeatherConfigServer();
void ensureWeatherConfigServer();
void handleProvisioningRoot();
void handleProvisioningWifiScan();
void handleProvisioningWifiSave();
void handleProvisioningRedirect();
void startProvisioningServer();
void startProvisioningMode();
void logWeatherConfigUrls();
void hideScreensaver(bool user_wake);
bool isSetupAccessPointVisible();
void setWifiSetupInfoVisible(bool visible, bool force_main_tile);
void homeSpanStatusCallback(HS_STATUS status);
void updateOledProtection();
void applyTimezoneSetting(bool request_sync);
void ensureTimeIsConfigured();
void updateTimezoneButtonLabel();
void updateTimezoneStatusLabel();
void updateSettingsWebLinksLabel();
void openTimezoneModal();
void closeTimezoneModal();
String weatherConfigStationUrl();
String weatherConfigApUrl();
void updateWeatherSetupButtonLabel();
void updateWeatherSetupModalContent();
void openWeatherSetupModal();
void closeWeatherSetupModal();
void timezone_button_event_handler(lv_event_t *e);
void timezone_roller_event_handler(lv_event_t *e);
void timezone_done_event_handler(lv_event_t *e);
void weather_setup_button_event_handler(lv_event_t *e);
void weather_setup_done_event_handler(lv_event_t *e);
void scheduleReboot(uint32_t delay_ms = kDeferredRebootMs);
void processScheduledReboot();

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

WeatherLocation selectedWeatherLocation()
{
    if (hasCustomWeatherLocation()) {
        return {weather_custom_latitude, weather_custom_longitude};
    }

    if (timezone_index < 0 || timezone_index >= kNumTimezones) {
        timezone_index = kDefaultTimezoneIndex;
    }
    return kWeatherLocations[timezone_index];
}

String selectedWeatherLocationLabel()
{
    if (hasCustomWeatherLocation()) {
        if (weather_location_name.length() > 0) {
            return weather_location_name;
        }
        return String("Custom location");
    }

    return String(selectedTimezoneLabel());
}

bool isBigSwitchOn()
{
    if (ui_big_button) {
        return lv_obj_has_state(ui_big_button, LV_STATE_CHECKED);
    }

    if (ui_master_clock_button) {
        return lv_obj_has_state(ui_master_clock_button, LV_STATE_CHECKED);
    }

    if (homekit_switches[8]) {
        return homekit_switches[8]->on->getVal();
    }

    return false;
}

bool isMasterClockTileActive()
{
    return tileview && tileMasterClock && lv_tileview_get_tile_act(tileview) == tileMasterClock;
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
    if (screensaver_weather_temp_label) {
        lv_obj_set_style_text_color(screensaver_weather_temp_label, color, 0);
    }
    if (screensaver_weather_sun_core) {
        lv_obj_set_style_bg_color(screensaver_weather_sun_core, color, 0);
    }
    for (lv_obj_t *ray : screensaver_weather_sun_rays) {
        if (ray) {
            lv_obj_set_style_line_color(ray, color, 0);
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

void updateMasterClockAppearance()
{
    if (!ui_master_clock_button) {
        return;
    }

    const bool inverted = isBigSwitchOn();
    const lv_color_t accent = inverted ? lv_color_black() : active_switch_color;
    const lv_color_t background = inverted ? active_switch_color : lv_color_black();

    lv_obj_set_style_bg_color(ui_master_clock_button, background, 0);
    lv_obj_set_style_bg_color(ui_master_clock_button, background, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(ui_master_clock_button, background, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui_master_clock_button, background, LV_STATE_CHECKED | LV_STATE_PRESSED);

    if (master_hour_hand) {
        lv_obj_set_style_line_color(master_hour_hand, accent, 0);
    }
    if (master_minute_hand) {
        lv_obj_set_style_line_color(master_minute_hand, accent, 0);
    }
    if (master_second_hand) {
        lv_obj_set_style_line_color(master_second_hand, accent, 0);
    }
    if (master_center_dot) {
        lv_obj_set_style_bg_color(master_center_dot, accent, 0);
    }
    if (master_weather_temp_label) {
        lv_obj_set_style_text_color(master_weather_temp_label, accent, 0);
    }
    if (master_weather_sun_core) {
        lv_obj_set_style_bg_color(master_weather_sun_core, accent, 0);
    }
    for (lv_obj_t *ray : master_weather_sun_rays) {
        if (ray) {
            lv_obj_set_style_line_color(ray, accent, 0);
        }
    }
    for (lv_obj_t *cloud_part : master_weather_cloud_parts) {
        if (cloud_part) {
            lv_obj_set_style_bg_color(cloud_part, accent, 0);
        }
    }
    for (lv_obj_t *rain_line : master_weather_rain_lines) {
        if (rain_line) {
            lv_obj_set_style_bg_color(rain_line, accent, 0);
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

void disableDecorationInteraction(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
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
    if (tileMasterClock) {
        hideScrollbarVisuals(tileMasterClock);
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

String weatherConfigStationUrl()
{
    return adminMdnsUrl();
}

String weatherConfigApUrl()
{
    return setupPortalUrl();
}

void updateSettingsWebLinksLabel()
{
    if (!ui_settings_web_links_label) {
        return;
    }

    const String mdns_url = adminMdnsUrl();
    const String direct_url = adminDirectUrl();
    const String setup_url = setupPortalUrl();
    String body;
    body.reserve(260);
    body += "Web UI:\n";

    if (mdns_url.length() > 0) {
        body += mdns_url;
        body += "\n";
    }
    if (direct_url.length() > 0) {
        body += "Alternative:\n";
        body += direct_url;
        body += "\n";
    }
    if (setup_url.length() > 0) {
        body += "Setup:\n";
        body += setup_url;
        body += "\n";
    }
    if (mdns_url.length() == 0 && direct_url.length() == 0 && setup_url.length() == 0) {
        body += "Connect Wi-Fi first.";
    }

    lv_label_set_text(ui_settings_web_links_label, body.c_str());
}

void updateWeatherSetupButtonLabel()
{
    if (!ui_weather_setup_value_label) {
        return;
    }

    lv_label_set_text(ui_weather_setup_value_label, "Web UI " LV_SYMBOL_RIGHT);
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

bool extractJsonStringField(const String &json, const char *key, String &value)
{
    const String pattern = String("\"") + key + "\":\"";
    int start = json.indexOf(pattern);
    if (start < 0) {
        return false;
    }

    start += pattern.length();
    String parsed;
    parsed.reserve(32);
    bool escape = false;

    for (int i = start; i < json.length(); ++i) {
        const char c = json[i];

        if (escape) {
            switch (c) {
            case '"':
            case '\\':
            case '/':
                parsed += c;
                break;
            case 'b':
                parsed += '\b';
                break;
            case 'f':
                parsed += '\f';
                break;
            case 'n':
                parsed += '\n';
                break;
            case 'r':
                parsed += '\r';
                break;
            case 't':
                parsed += '\t';
                break;
            default:
                parsed += c;
                break;
            }
            escape = false;
            continue;
        }

        if (c == '\\') {
            escape = true;
            continue;
        }

        if (c == '"') {
            value = parsed;
            return true;
        }

        parsed += c;
    }

    return false;
}

bool parseFloatParameter(const String &text, float &value)
{
    String trimmed = text;
    trimmed.trim();
    if (trimmed.isEmpty()) {
        return false;
    }

    trimmed.replace(',', '.');

    char *parse_end = nullptr;
    value = strtof(trimmed.c_str(), &parse_end);
    return parse_end != trimmed.c_str() && parse_end && *parse_end == '\0';
}

String urlEncode(const String &text)
{
    String encoded;
    encoded.reserve(text.length() * 3);

    for (size_t i = 0; i < text.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(text[i]);
        const bool is_unreserved =
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~';

        if (is_unreserved) {
            encoded += static_cast<char>(c);
            continue;
        }

        if (c == ' ') {
            encoded += "%20";
            continue;
        }

        char buffer[4];
        snprintf(buffer, sizeof(buffer), "%02X", c);
        encoded += '%';
        encoded += buffer;
    }

    return encoded;
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

void updateWeatherSetupModalContent()
{
    if (!ui_weather_setup_modal_text) {
        return;
    }

    const String station_url = weatherConfigStationUrl();
    const String direct_url = adminDirectUrl();
    const String ap_url = weatherConfigApUrl();
    String body;
    body.reserve(320);
    body += "Open the admin web UI in your browser to change\n";
    body += "device, display, time and weather settings.";
    body += "\n\n";

    if (station_url.length() > 0) {
        body += "Primary URL\n";
        body += station_url;
        body += "\n\n";
    }

    if (direct_url.length() > 0) {
        body += "Alternative URL\n";
        body += direct_url;
        body += "\n\n";
    }

    if (ap_url.length() > 0) {
        body += "Setup Portal\n";
        body += ap_url;
        body += "\n\n";
    }

    if (station_url.length() == 0 && direct_url.length() == 0 && ap_url.length() == 0) {
        body += "Connect Wi-Fi or enter setup mode first.";
    } else {
        body += "Use the Weather section in the admin UI to update\n";
        body += "the clock weather monogram.";
    }

    lv_label_set_text(ui_weather_setup_modal_text, body.c_str());
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

const char *weatherGlyphLabel(WeatherGlyph glyph)
{
    switch (glyph) {
    case WeatherGlyph::Sun:
        return "Sunny";
    case WeatherGlyph::Cloud:
        return "Cloudy";
    case WeatherGlyph::Rain:
        return "Rain";
    }

    return "Weather";
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

void setMasterClockWeatherGlyph(WeatherGlyph glyph)
{
    const bool show_sun = (glyph == WeatherGlyph::Sun);
    const bool show_cloud = (glyph == WeatherGlyph::Cloud || glyph == WeatherGlyph::Rain);
    const bool show_rain = (glyph == WeatherGlyph::Rain);

    if (master_weather_sun_core) {
        if (show_sun) {
            lv_obj_clear_flag(master_weather_sun_core, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(master_weather_sun_core, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (lv_obj_t *ray : master_weather_sun_rays) {
        if (!ray) {
            continue;
        }
        if (show_sun) {
            lv_obj_clear_flag(ray, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ray, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (lv_obj_t *cloud_part : master_weather_cloud_parts) {
        if (!cloud_part) {
            continue;
        }
        if (show_cloud) {
            lv_obj_clear_flag(cloud_part, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(cloud_part, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (lv_obj_t *rain_line : master_weather_rain_lines) {
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

void updateMasterClock(bool force)
{
    if (!ui_master_clock_button || !master_clock_face || !master_hour_hand || !master_minute_hand || !master_second_hand) {
        return;
    }

    if (lv_obj_has_flag(ui_master_clock_button, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    if (!isMasterClockTileActive()) {
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

    const lv_coord_t face_size = lv_obj_get_width(master_clock_face);
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

    setClockHandPoints(master_hour_points, hour_angle, cx, cy, hour_length, hour_tail);
    setClockHandPoints(master_minute_points, minute_angle, cx, cy, minute_length, minute_tail);
    setClockHandPoints(master_second_points, second_angle, cx, cy, second_length, second_tail);

    lv_line_set_points(master_hour_hand, master_hour_points, 2);
    lv_line_set_points(master_minute_hand, master_minute_points, 2);
    lv_line_set_points(master_second_hand, master_second_points, 2);

    if (!master_weather_group || !master_weather_temp_label) {
        return;
    }

    if (!shouldShowClockWeather()) {
        lv_obj_add_flag(master_weather_group, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    setMasterClockWeatherGlyph(glyphForWeatherCode(weather_code, weather_is_day));
    lv_label_set_text_fmt(master_weather_temp_label, "%d°", weather_temperature_c);

    const float hour_angle_rad = hour_angle * static_cast<float>(PI) / 180.0f;
    const bool hour_hand_in_top_half = sinf(hour_angle_rad) < 0.0f;
    const lv_coord_t radial_midpoint_offset = lv_obj_get_height(master_clock_face) / 4;

    lv_obj_clear_flag(master_weather_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(
        master_weather_group,
        LV_ALIGN_CENTER,
        0,
        hour_hand_in_top_half ? radial_midpoint_offset : -radial_midpoint_offset);
}

void updateMasterClockMode()
{
    if (!ui_master_clock_button || !ui_master_clock_info) {
        return;
    }

    if (isClockButtonEnabled()) {
        lv_obj_add_flag(ui_master_clock_info, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_master_clock_button, LV_OBJ_FLAG_HIDDEN);
        updateMasterClockAppearance();
        if (isMasterClockTileActive()) {
            updateMasterClock(true);
        }
    } else {
        lv_obj_add_flag(ui_master_clock_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_master_clock_info, LV_OBJ_FLAG_HIDDEN);
    }
}

void updateWeatherMonogram()
{
    if (screensaver_weather_group && screensaver_weather_temp_label) {
        if (!shouldShowClockWeather()) {
            lv_obj_add_flag(screensaver_weather_group, LV_OBJ_FLAG_HIDDEN);
        } else {
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
    }

    if (isMasterClockTileActive()) {
        updateMasterClock(true);
    }
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

    closeWeatherSetupModal();
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

void openWeatherSetupModal()
{
    if (!ui_weather_setup_modal) {
        return;
    }

    closeTimezoneModal();
    updateWeatherSetupModalContent();
    lv_obj_move_foreground(ui_weather_setup_modal);
    lv_obj_clear_flag(ui_weather_setup_modal, LV_OBJ_FLAG_HIDDEN);
    lv_disp_trig_activity(nullptr);
}

void closeWeatherSetupModal()
{
    if (!ui_weather_setup_modal) {
        return;
    }

    lv_obj_add_flag(ui_weather_setup_modal, LV_OBJ_FLAG_HIDDEN);
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
    updateWeatherSetupButtonLabel();
    updateWeatherSetupModalContent();
    updateSettingsWebLinksLabel();
}

bool fetchCurrentWeather()
{
    if (WiFi.status() != WL_CONNECTED || !isWeatherEnabled()) {
        return false;
    }

    const WeatherLocation location = selectedWeatherLocation();
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

    if (wifi_status != WL_CONNECTED || !isWeatherEnabled()) {
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
    if (isWeatherEnabled()) {
        fetchCurrentWeather();
    }
}

int parseWeatherSearchResults(const String &json, WeatherSearchResult results[], int max_results)
{
    if (!results || max_results <= 0) {
        return 0;
    }

    const int results_key = json.indexOf("\"results\":");
    if (results_key < 0) {
        return 0;
    }

    const int array_start = json.indexOf('[', results_key);
    if (array_start < 0) {
        return 0;
    }

    int result_count = 0;
    int array_depth = 1;
    int object_depth = 0;
    int object_start = -1;
    bool in_string = false;
    bool escape = false;

    for (int i = array_start + 1; i < json.length() && array_depth > 0; ++i) {
        const char c = json[i];

        if (escape) {
            escape = false;
            continue;
        }

        if (c == '\\' && in_string) {
            escape = true;
            continue;
        }

        if (c == '"') {
            in_string = !in_string;
            continue;
        }

        if (in_string) {
            continue;
        }

        if (c == '[') {
            ++array_depth;
            continue;
        }

        if (c == ']') {
            --array_depth;
            continue;
        }

        if (c == '{') {
            if (array_depth == 1 && object_depth == 0) {
                object_start = i;
            }
            ++object_depth;
            continue;
        }

        if (c == '}') {
            --object_depth;
            if (array_depth == 1 && object_depth == 0 && object_start >= 0) {
                WeatherSearchResult result;
                const String object_json = json.substring(object_start, i + 1);

                String name;
                String admin1;
                String country;
                float latitude = 0.0f;
                float longitude = 0.0f;

                if (extractJsonStringField(object_json, "name", name) &&
                    extractJsonNumberField(object_json, "latitude", latitude) &&
                    extractJsonNumberField(object_json, "longitude", longitude) &&
                    isValidWeatherCoordinates(latitude, longitude)) {
                    extractJsonStringField(object_json, "admin1", admin1);
                    extractJsonStringField(object_json, "country", country);

                    result.name = name;
                    result.admin1 = admin1;
                    result.country = country;
                    result.latitude = latitude;
                    result.longitude = longitude;
                    result.valid = true;
                    results[result_count++] = result;

                    if (result_count >= max_results) {
                        break;
                    }
                }

                object_start = -1;
            }
        }
    }

    return result_count;
}

bool fetchWeatherSearchResults(
    const String &query,
    WeatherSearchResult results[],
    int max_results,
    int &result_count,
    String &error_message)
{
    result_count = 0;
    error_message = "";

    if (WiFi.status() != WL_CONNECTED) {
        error_message = "City search needs a Wi-Fi internet connection.";
        return false;
    }

    String trimmed_query = query;
    trimmed_query.trim();
    if (trimmed_query.length() < 2) {
        error_message = "Please enter at least 2 characters.";
        return false;
    }

    String url = "https://geocoding-api.open-meteo.com/v1/search?name=";
    url += urlEncode(trimmed_query);
    url += "&count=";
    url += String(max_results);
    url += "&language=de&format=json";

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(client, url)) {
        error_message = "Unable to start the city search request.";
        return false;
    }

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        http.end();
        error_message = "City search failed. Please try again in a moment.";
        return false;
    }

    const String payload = http.getString();
    http.end();

    result_count = parseWeatherSearchResults(payload, results, max_results);
    if (result_count <= 0) {
        error_message = "No matching city or village was found.";
        return false;
    }

    return true;
}

String htmlEscape(const String &text)
{
    String escaped;
    escaped.reserve(text.length() + 16);

    for (size_t i = 0; i < text.length(); ++i) {
        switch (text[i]) {
        case '&':
            escaped += F("&amp;");
            break;
        case '<':
            escaped += F("&lt;");
            break;
        case '>':
            escaped += F("&gt;");
            break;
        case '"':
            escaped += F("&quot;");
            break;
        case '\'':
            escaped += F("&#39;");
            break;
        default:
            escaped += text[i];
            break;
        }
    }

    return escaped;
}

String jsonEscape(const String &text)
{
    String escaped;
    escaped.reserve(text.length() + 8);

    for (size_t i = 0; i < text.length(); ++i) {
        const char c = text[i];
        switch (c) {
        case '\\':
            escaped += F("\\\\");
            break;
        case '"':
            escaped += F("\\\"");
            break;
        case '\n':
            escaped += F("\\n");
            break;
        case '\r':
            escaped += F("\\r");
            break;
        case '\t':
            escaped += F("\\t");
            break;
        default:
            escaped += c;
            break;
        }
    }

    return escaped;
}

String weatherSearchDisplayName(const WeatherSearchResult &result)
{
    String label = result.name;

    if (result.admin1.length() > 0 && result.admin1 != result.name) {
        label += ", ";
        label += result.admin1;
    }

    if (result.country.length() > 0) {
        label += ", ";
        label += result.country;
    }

    return label;
}

String defaultDeviceHostName()
{
    String suffix = BRIDGE_SUFFIX_STR;
    suffix.toLowerCase();
    if (suffix.isEmpty()) {
        suffix = "0000";
    }
    return String(kDefaultHostBase) + "-" + suffix;
}

String defaultDeviceLabel()
{
    if (BRIDGE_SUFFIX_STR.isEmpty()) {
        return String("HOMEsmthng");
    }
    return String("HOMEsmthng ") + BRIDGE_SUFFIX_STR;
}

String sanitizeHostName(const String &text)
{
    String value = text;
    value.trim();
    value.toLowerCase();

    String sanitized;
    sanitized.reserve(value.length());
    bool last_was_dash = false;

    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        const bool is_alpha_numeric =
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9');

        if (is_alpha_numeric) {
            sanitized += c;
            last_was_dash = false;
            continue;
        }

        if (!last_was_dash && sanitized.length() > 0) {
            sanitized += '-';
            last_was_dash = true;
        }
    }

    while (sanitized.startsWith("-")) {
        sanitized.remove(0, 1);
    }
    while (sanitized.endsWith("-")) {
        sanitized.remove(sanitized.length() - 1);
    }

    if (sanitized.isEmpty()) {
        return defaultDeviceHostName();
    }

    return sanitized;
}

String formattedPairingCode()
{
    if (HOMEKIT_PAIRING_CODE_STR.length() != 8) {
        return HOMEKIT_PAIRING_CODE_STR;
    }
    return HOMEKIT_PAIRING_CODE_STR.substring(0, 4) + "-" + HOMEKIT_PAIRING_CODE_STR.substring(4, 8);
}

String setupAccessPointSsid()
{
    return String("homesmthng");
}

String setupPortalUrl()
{
    if (!provisioning_mode_active) {
        return String();
    }
    return String("http://192.168.4.1");
}

String adminMdnsUrl()
{
    if (WiFi.status() != WL_CONNECTED || device_host_name.isEmpty()) {
        return String();
    }
    return String("http://") + device_host_name + ".local:" + String(kAdminWebPort);
}

String adminDirectUrl()
{
    if (WiFi.status() != WL_CONNECTED) {
        return String();
    }

    const IPAddress station_ip = WiFi.localIP();
    if (station_ip[0] == 0 && station_ip[1] == 0 && station_ip[2] == 0 && station_ip[3] == 0) {
        return String();
    }

    return String("http://") + station_ip.toString() + ":" + String(kAdminWebPort);
}

bool isPairingOnboardingActive()
{
    return homespan_started && WiFi.status() == WL_CONNECTED && !homekit_is_paired;
}

String defaultAdminSection(const String &requested_section)
{
    if (requested_section.length() > 0) {
        return requested_section;
    }

    if (isPairingOnboardingActive()) {
        return String("homekit");
    }

    return String();
}

bool readStoredWifiCredentials(StoredWifiCredentials &credentials)
{
    memset(&credentials, 0, sizeof(credentials));

    nvs_handle handle = 0;
    if (nvs_open(kHomeSpanWifiNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t len = sizeof(credentials);
    const esp_err_t err = nvs_get_blob(handle, kHomeSpanWifiKey, &credentials, &len);
    nvs_close(handle);

    if (err != ESP_OK || len < sizeof(credentials.ssid)) {
        memset(&credentials, 0, sizeof(credentials));
        return false;
    }

    credentials.ssid[MAX_SSID] = '\0';
    credentials.pwd[MAX_PWD] = '\0';
    return strlen(credentials.ssid) > 0;
}

bool saveStoredWifiCredentials(const StoredWifiCredentials &credentials)
{
    nvs_handle handle = 0;
    if (nvs_open(kHomeSpanWifiNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    const esp_err_t set_err = nvs_set_blob(handle, kHomeSpanWifiKey, &credentials, sizeof(credentials));
    const esp_err_t commit_err = (set_err == ESP_OK) ? nvs_commit(handle) : set_err;
    nvs_close(handle);
    return commit_err == ESP_OK;
}

void eraseStoredWifiCredentials()
{
    nvs_handle handle = 0;
    if (nvs_open(kHomeSpanWifiNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }

    nvs_erase_key(handle, kHomeSpanWifiKey);
    nvs_commit(handle);
    nvs_close(handle);
}

void loadOrCreateDeviceConfig()
{
    preferences.begin("homespan", false);

    String stored_host = preferences.getString("device_host", "");
    String stored_label = preferences.getString("device_label", "");

    stored_host = sanitizeHostName(stored_host);
    if (stored_host.isEmpty()) {
        stored_host = defaultDeviceHostName();
    }

    stored_label.trim();
    if (stored_label.isEmpty()) {
        stored_label = defaultDeviceLabel();
    }

    device_host_name = stored_host;
    device_label = stored_label;

    preferences.putString("device_host", device_host_name);
    preferences.putString("device_label", device_label);
    preferences.end();
}

void saveDeviceConfigToNVS()
{
    preferences.begin("homespan", false);
    preferences.putString("device_host", device_host_name);
    preferences.putString("device_label", device_label);
    preferences.end();
}

void applyBrightnessSetting(int brightness, bool persist)
{
    global_brightness = constrain(brightness, 1, boardProfile().brightness_levels);
    if (ui_brightness_slider) {
        lv_slider_set_value(ui_brightness_slider, global_brightness, LV_ANIM_OFF);
    }
    safeSetBrightness(global_brightness);

    if (persist) {
        saveSettingsToNVS();
    }
}

int brightnessScaleForWeb(int brightness)
{
    const int max_level = max(1, boardProfile().brightness_levels);
    const int clamped_brightness = constrain(brightness, 1, max_level);
    if (max_level <= 1) {
        return kWebBrightnessScaleMax;
    }

    return static_cast<int>(lroundf(
        (static_cast<float>(clamped_brightness - 1) * static_cast<float>(kWebBrightnessScaleMax)) /
        static_cast<float>(max_level - 1)));
}

int brightnessLevelFromWebScale(int scale_value)
{
    const int max_level = max(1, boardProfile().brightness_levels);
    const int clamped_scale = constrain(scale_value, 0, kWebBrightnessScaleMax);
    if (max_level <= 1) {
        return 1;
    }

    return 1 + static_cast<int>(lroundf(
                   (static_cast<float>(clamped_scale) * static_cast<float>(max_level - 1)) /
                   static_cast<float>(kWebBrightnessScaleMax)));
}

bool parseHexColor(const String &text, uint32_t &color_hex)
{
    String value = text;
    value.trim();
    if (value.startsWith("#")) {
        value.remove(0, 1);
    }

    if (value.length() != 6) {
        return false;
    }

    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        const bool is_hex =
            (c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F');
        if (!is_hex) {
            return false;
        }
    }

    color_hex = static_cast<uint32_t>(strtoul(value.c_str(), nullptr, 16)) & 0xFFFFFF;
    return true;
}

String colorHexCss(uint32_t color_hex)
{
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "#%06lX", static_cast<unsigned long>(color_hex & 0xFFFFFF));
    return String(buffer);
}

void applyScreen2BacklightSetting(bool enabled, bool persist)
{
    screen2_bl_always_on = enabled;
    if (ui_bl_toggle) {
        if (!enabled) {
            lv_obj_add_state(ui_bl_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ui_bl_toggle, LV_STATE_CHECKED);
        }
    }
    syncScreen2IdleTimer();

    if (persist) {
        saveSettingsToNVS();
    }
}

void applyPixelShiftSetting(bool enabled, bool persist)
{
    oled_pixel_shift_enabled = supportsPixelShift() && enabled;
    if (ui_pixel_shift_toggle) {
        if (oled_pixel_shift_enabled) {
            lv_obj_add_state(ui_pixel_shift_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ui_pixel_shift_toggle, LV_STATE_CHECKED);
        }
    }
    if (!oled_pixel_shift_enabled) {
        applyPixelShiftOffset(0, 0);
    }

    if (persist) {
        saveSettingsToNVS();
    }
}

void applyClockButtonSetting(bool enabled, bool persist)
{
    clock_button_screen_enabled = supportsClockSaver() && enabled;
    if (ui_clock_button_toggle) {
        if (clock_button_screen_enabled) {
            lv_obj_add_state(ui_clock_button_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ui_clock_button_toggle, LV_STATE_CHECKED);
        }
    }
    updateMasterClockMode();

    if (persist) {
        saveSettingsToNVS();
    }
}

void applyClockSaverSetting(bool enabled, bool persist)
{
    oled_clock_saver_enabled = supportsClockSaver() && enabled;
    if (ui_clock_saver_toggle) {
        if (oled_clock_saver_enabled) {
            lv_obj_add_state(ui_clock_saver_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ui_clock_saver_toggle, LV_STATE_CHECKED);
        }
    }
    if (!oled_clock_saver_enabled) {
        hideScreensaver(false);
    }

    if (persist) {
        saveSettingsToNVS();
    }
}

void applyClockSaverIdleSetting(uint32_t idle_ms, bool persist)
{
    clock_saver_idle_ms = constrain(idle_ms, kClockSaverMinIdleMs, kClockSaverMaxIdleMs);
    if (persist) {
        saveSettingsToNVS();
    }
}

void applyTimezoneSettingIndex(int index, bool persist)
{
    timezone_index = constrain(index, 0, kNumTimezones - 1);
    if (ui_timezone_roller) {
        lv_roller_set_selected(ui_timezone_roller, static_cast<uint16_t>(timezone_index), LV_ANIM_OFF);
    }

    applyTimezoneSetting(true);
    if (screensaver_visible) {
        updateScreensaverClock(true);
    }
    updateMasterClock(true);

    if (persist) {
        saveSettingsToNVS();
    }
}

void applyWeatherEnabledSetting(bool enabled, bool persist)
{
    weather_enabled = enabled;
    if (!isWeatherEnabled()) {
        weather_has_data = false;
        weather_refresh_pending = false;
    } else {
        weather_refresh_pending = true;
        weather_last_request_ms = 0;
    }
    updateWeatherMonogram();

    if (persist) {
        saveSettingsToNVS();
    }
}

void deferWeatherRefresh(uint32_t delay_ms)
{
    weather_refresh_pending = true;
    const uint32_t remaining_retry_ms = delay_ms < kWeatherRetryMs ? (kWeatherRetryMs - delay_ms) : 0;
    weather_last_request_ms = millis() - remaining_retry_ms;
}

void saveCustomWeatherLocation(const String &name, float latitude, float longitude, bool persist)
{
    weather_use_custom_location = true;
    weather_location_name = name.length() > 0 ? name : String("Custom location");
    weather_custom_latitude = latitude;
    weather_custom_longitude = longitude;
    weather_has_data = false;
    deferWeatherRefresh(10000);
    updateWeatherMonogram();

    if (persist) {
        saveSettingsToNVS();
    }
}

void resetCustomWeatherLocation(bool persist)
{
    weather_use_custom_location = false;
    weather_location_name = "";
    weather_custom_latitude = 0.0f;
    weather_custom_longitude = 0.0f;
    weather_has_data = false;
    weather_refresh_pending = false;
    weather_last_request_ms = millis();
    updateWeatherMonogram();

    if (persist) {
        saveSettingsToNVS();
    }
}

void updateWifiSetupMessage()
{
    if (!ui_wifi_setup_msg) {
        return;
    }

    String body;
    body.reserve(256);

    if (provisioning_mode_active) {
        body += "Connect to Wi-Fi hotspot\n";
        body += setupAccessPointSsid();
    } else if (isPairingOnboardingActive()) {
        const String mdns_url = adminMdnsUrl();
        const String direct_url = adminDirectUrl();
        const String browser_url = (mdns_url.length() > 0)
                                       ? mdns_url
                                       : String("http://") + device_host_name + ".local:" + String(kAdminWebPort);
        body += "Connect to your smart home\n\n";
        body += "Open in browser\n";
        body += browser_url;
        if (direct_url.length() > 0) {
            body += "\n\nAlternative\n";
            body += direct_url;
        }
    } else {
        body += "";
    }

    lv_label_set_text(ui_wifi_setup_msg, body.c_str());
}

String homeKitSetupPayload()
{
    HapQR qr;
    return String(qr.get(static_cast<uint32_t>(HOMEKIT_PAIRING_CODE_STR.toInt()), kHomeKitSetupId, static_cast<uint8_t>(Category::Bridges)));
}

String renderQrSvg(const String &payload)
{
    if (payload.isEmpty()) {
        return String();
    }

    const int version = qrcodegen_getMinFitVersion(qrcodegen_Ecc_MEDIUM, payload.length());
    if (version <= 0) {
        return String();
    }

    const size_t buffer_len = qrcodegen_BUFFER_LEN_FOR_VERSION(version);
    std::unique_ptr<uint8_t[]> temp_buffer(new uint8_t[buffer_len]);
    std::unique_ptr<uint8_t[]> qr_buffer(new uint8_t[buffer_len]);
    if (!temp_buffer || !qr_buffer) {
        return String();
    }

    if (!qrcodegen_encodeText(
            payload.c_str(),
            temp_buffer.get(),
            qr_buffer.get(),
            qrcodegen_Ecc_MEDIUM,
            version,
            version,
            qrcodegen_Mask_AUTO,
            true)) {
        return String();
    }

    const int qr_size = qrcodegen_getSize(qr_buffer.get());
    const int border = 4;
    const int canvas = qr_size + border * 2;

    String svg;
    svg.reserve(6000);
    svg += F("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    svg += "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " + String(canvas) + " " + String(canvas) + "\" shape-rendering=\"crispEdges\">";
    svg += F("<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>");
    svg += F("<path d=\"");

    for (int y = 0; y < qr_size; ++y) {
        for (int x = 0; x < qr_size; ++x) {
            if (!qrcodegen_getModule(qr_buffer.get(), x, y)) {
                continue;
            }
            svg += "M";
            svg += String(x + border);
            svg += " ";
            svg += String(y + border);
            svg += "h1v1h-1z";
        }
    }

    svg += F("\" fill=\"#000000\"/></svg>");
    return svg;
}

String renderWebHomePage()
{
    return renderAdminPage();
}

String renderWeatherConfigPage(
    const String &message,
    const String &search_query,
    const WeatherSearchResult *search_results,
    int search_result_count,
    const String &search_message)
{
    return renderAdminPage(message, false, search_query, search_results, search_result_count, search_message, false, "weather");
}

String renderAdminPage(
    const String &message,
    bool is_error,
    const String &search_query,
    const WeatherSearchResult *search_results,
    int search_result_count,
    const String &search_message,
    bool search_is_error,
    const String &active_section)
{
    const WeatherLocation current_location = selectedWeatherLocation();
    const String current_label = selectedWeatherLocationLabel();
    const bool weather_location_set = hasCustomWeatherLocation();
    String custom_name = weather_location_name;
    if (custom_name.length() == 0) {
        custom_name = hasCustomWeatherLocation() ? String("Custom location") : String();
    }

    StoredWifiCredentials stored_wifi = {};
    const bool has_stored_wifi = readStoredWifiCredentials(stored_wifi);
    const String stored_ssid = has_stored_wifi ? String(stored_wifi.ssid) : String();
    const String mdns_url = adminMdnsUrl();
    const String direct_url = adminDirectUrl();
    const String setup_url = setupPortalUrl();
    const String pairing_code = formattedPairingCode();
    const String pairing_payload = homeKitSetupPayload();
    const String escaped_label = htmlEscape(current_label);
    const String escaped_name = htmlEscape(custom_name);
    const String weather_search_value = search_query.length() > 0 ? search_query : (weather_location_set ? current_label : String());
    const String escaped_weather_search_value = htmlEscape(weather_search_value);
    const String escaped_message = htmlEscape(message);
    const String escaped_search_query = htmlEscape(search_query);
    const String escaped_search_message = htmlEscape(search_message);
    const String escaped_device_label = htmlEscape(device_label);
    const String escaped_device_host = htmlEscape(device_host_name);
    const String escaped_mdns_url = htmlEscape(mdns_url);
    const String escaped_direct_url = htmlEscape(direct_url);
    const String escaped_setup_url = htmlEscape(setup_url);
    const String escaped_stored_ssid = htmlEscape(stored_ssid);
    const String active_color_css = colorHexCss(active_switch_color_hex);
    const int brightness_scale_value = brightnessScaleForWeb(global_brightness);
    String normalized_active_section = active_section;
    normalized_active_section.toLowerCase();
    const bool open_homekit = normalized_active_section == "homekit";
    const bool open_device = normalized_active_section == "device";
    const bool open_wifi = normalized_active_section == "wifi";
    const bool open_display = normalized_active_section == "display";
    const bool open_time = normalized_active_section == "time";
    const bool open_weather = normalized_active_section == "weather";

    String weather_status = "Set a weather location to show weather";
    if (!weather_enabled) {
        weather_status = "Disabled";
    } else if (!weather_location_set) {
        weather_status = "No location set";
    } else if (weather_has_data) {
        weather_status = String(weather_temperature_c) + "&deg;C &middot; " + weatherGlyphLabel(glyphForWeatherCode(weather_code, weather_is_day));
    } else if (WiFi.status() != WL_CONNECTED) {
        weather_status = "Weather fetch needs Wi-Fi";
    } else {
        weather_status = "Waiting for weather data";
    }

    String wifi_status = "Not connected";
    if (WiFi.status() == WL_CONNECTED) {
        wifi_status = WiFi.SSID();
        if (wifi_status.length() == 0) {
            wifi_status = "Connected";
        }
    } else if (has_stored_wifi) {
        wifi_status = "Saved network: " + stored_ssid;
    }

    String page;
    page.reserve(30000);
    page += F(
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>HOMEsmthng Admin</title>"
        "<style>"
        ":root{color-scheme:dark;--bg:#0c0f10;--surface:#171b1d;--surface-2:#111517;--border:#2a2f33;--border-strong:#40484d;--text:#f4f4f4;--muted:#c7cccf;--subtle:#98a1a6;--input:#0f1315;--primary:#68724D;--secondary:#273038;--danger:#3a2020;--msg:#1f2d24;--msg-text:#d7f2df;--error:#3a2020;--error-text:#ffd6d6;--shadow:0 18px 50px rgba(0,0,0,.22);}"
        "@media(prefers-color-scheme:light){:root{color-scheme:light;--bg:#f5f2eb;--surface:#fffdf8;--surface-2:#f4f0e7;--border:#ded8cc;--border-strong:#bdb5a6;--text:#171b1d;--muted:#4b5257;--subtle:#737b80;--input:#ffffff;--primary:#68724D;--secondary:#e8e1d3;--danger:#f2d7d4;--msg:#e4efdc;--msg-text:#28361f;--error:#f2d7d4;--error-text:#5b1e18;--shadow:0 18px 50px rgba(74,63,44,.14);}}"
        "*{transition:background-color .18s ease,border-color .18s ease,color .18s ease,box-shadow .18s ease,transform .18s ease;}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:radial-gradient(circle at top left,color-mix(in srgb,var(--primary) 18%,transparent),transparent 34rem),var(--bg);color:var(--text);margin:0;padding:24px;}"
        ".wrap{max-width:720px;margin:0 auto;}"
        ".card{background:var(--surface);border:1px solid var(--border);border-radius:18px;padding:20px;margin-bottom:18px;box-shadow:0 0 0 rgba(0,0,0,0);}"
        ".card:hover{border-color:var(--border-strong);box-shadow:var(--shadow);transform:translateY(-1px);}"
        "h1,h2{margin:0 0 12px 0;}p{color:var(--muted);line-height:1.45;}"
        "label{display:block;margin:14px 0 6px 0;font-weight:600;}"
        "input,select{width:100%;box-sizing:border-box;border-radius:12px;border:1px solid var(--border-strong);background:var(--input);color:var(--text);padding:14px;font-size:16px;}"
        "input:hover,select:hover{border-color:var(--primary);}input:focus,select:focus,button:focus-visible,summary:focus-visible{outline:2px solid color-mix(in srgb,var(--primary) 70%,white);outline-offset:2px;}"
        ".row{display:flex;gap:12px;flex-wrap:wrap;}.row > div{flex:1 1 220px;}"
        ".actions{display:flex;gap:12px;flex-wrap:wrap;margin-top:18px;}"
        "button{border:0;border-radius:999px;padding:12px 18px;font-size:15px;font-weight:600;cursor:pointer;}"
        "button:hover{transform:translateY(-1px);filter:brightness(1.08);}button:active{transform:translateY(0) scale(.98);}"
        ".primary{background:var(--primary);color:#fff;}.secondary{background:var(--secondary);color:var(--text);}.danger{background:var(--danger);color:var(--text);}"
        ".msg{margin-bottom:16px;padding:12px 14px;border-radius:12px;background:var(--msg);color:var(--msg-text);}"
        ".msg.error{background:var(--error);color:var(--error-text);}"
        ".search-results{display:flex;flex-direction:column;gap:10px;margin-top:16px;}"
        ".result-form{margin:0;}"
        ".result-btn{width:100%;text-align:left;background:var(--surface-2);border:1px solid var(--border);border-radius:14px;padding:14px 16px;color:var(--text);}"
        ".result-btn:hover{border-color:var(--primary);box-shadow:0 10px 24px color-mix(in srgb,var(--primary) 16%,transparent);}"
        ".result-title{display:block;font-size:16px;font-weight:700;margin-bottom:4px;}"
        ".result-meta{display:block;color:var(--muted);font-size:14px;line-height:1.35;}"
        ".location-search{display:flex;gap:10px;align-items:stretch;}"
        ".location-search input{flex:1 1 auto;min-width:0;}"
        ".pin-btn{flex:0 0 54px;border-radius:12px;padding:0;background:var(--secondary);color:var(--text);display:flex;align-items:center;justify-content:center;}"
        ".pin-btn svg{width:20px;height:20px;display:block;}"
        ".manual-location{margin-top:20px;border:1px solid var(--border);border-radius:14px;background:var(--surface-2);overflow:hidden;}"
        ".manual-location summary{padding:14px 16px;font-size:16px;font-weight:700;}"
        ".manual-location .manual-body{padding:0 16px 16px 16px;border-top:1px solid var(--border);}"
        ".hidden-form{display:none;}"
        ".weather-location-hint{display:none;margin-top:10px;padding:10px 12px;border-radius:12px;background:var(--error);color:var(--error-text);font-size:14px;line-height:1.4;}"
        ".weather-location-hint.show{display:block;}"
        ".subtle{color:var(--subtle);font-size:14px;}"
        ".mono{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:14px;color:var(--muted);overflow-wrap:anywhere;word-break:break-word;}"
        ".status{display:grid;grid-template-columns:minmax(140px,180px) 1fr;gap:8px 12px;}"
        ".status strong{color:var(--text);}"
        ".status span{min-width:0;overflow-wrap:anywhere;word-break:break-word;}"
        ".status-link{color:var(--text);text-decoration:none;border-bottom:1px solid var(--border-strong);}"
        ".status-link:hover{color:var(--primary);border-color:var(--primary);}"
        ".pairing-code{color:var(--text);}"
        ".checkbox-label{display:flex;align-items:center;gap:10px;margin:10px 0;font-weight:500;}"
        ".checkbox-label input{width:auto;flex:0 0 auto;padding:0;margin:0;}"
        ".toggle-item{margin:12px 0 16px 0;}"
        ".toggle-item .subtle{margin:2px 0 0 0;}"
        ".toggle-item .checkbox-label + .subtle{margin-left:34px;}"
        ".qr{display:flex;gap:20px;align-items:flex-start;flex-wrap:wrap;}"
        ".qr img{width:220px;height:220px;background:#fff;border-radius:18px;padding:10px;box-sizing:border-box;}"
        ".stepper{display:flex;gap:10px;align-items:center;}"
        ".stepper input{flex:1 1 auto;margin:0;}"
        ".step-btn{min-width:52px;padding:12px 0;line-height:1;font-size:22px;}"
        ".color-picker-wrap{position:relative;}"
        "input[type=\"color\"]{appearance:none;-webkit-appearance:none;padding:6px;height:58px;background:transparent;cursor:pointer;}"
        "input[type=\"color\"]::-webkit-color-swatch-wrapper{padding:0;}"
        "input[type=\"color\"]::-webkit-color-swatch{border:1px solid var(--border-strong);border-radius:10px;}"
        "input[type=\"color\"]::-moz-color-swatch{border:1px solid var(--border-strong);border-radius:10px;}"
        ".color-picker-plus{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;pointer-events:none;"
        "font-size:34px;font-weight:500;color:var(--text);text-shadow:0 1px 4px var(--bg),0 0 10px var(--bg);}"
        ".swatches{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px;}"
        ".swatch{width:42px;height:42px;border-radius:999px;border:2px solid var(--border-strong);padding:0;background-clip:padding-box;box-shadow:inset 0 0 0 1px rgba(0,0,0,.18);}"
        ".swatch:hover{transform:translateY(-2px) scale(1.04);box-shadow:0 8px 18px rgba(0,0,0,.18);}"
        ".swatch.active{border-color:var(--text);}"
        ".section-card{padding:0;overflow:hidden;}"
        ".section-card summary{list-style:none;cursor:pointer;padding:20px;font-size:18px;font-weight:700;}"
        ".section-card summary:hover{background:color-mix(in srgb,var(--primary) 10%,transparent);}"
        ".section-card summary::-webkit-details-marker{display:none;}"
        ".section-card summary::after{content:'›';float:right;color:var(--subtle);transition:transform .2s ease;}"
        ".section-card[open] summary::after{transform:rotate(90deg);}"
        ".section-body{padding:0 20px 20px 20px;border-top:1px solid var(--border);}"
        "@media(max-width:520px){body{padding:14px;}.card{padding:16px;}.status{grid-template-columns:1fr;gap:4px;}.status strong{margin-top:8px;}}"
        "</style></head><body><div class=\"wrap\">");

    if (message.length() > 0) {
        page += "<div class=\"msg";
        if (is_error) {
            page += " error";
        }
        page += "\">" + escaped_message + "</div>";
    }

    page += "<div class=\"card\"><h1>" + escaped_device_label + "</h1>";
    page += "<p>Admin UI for device setup, Wi-Fi, display settings, weather and Apple Home.</p>";
    page += "<div class=\"status\">";
    page += "<strong>Hostname</strong><span class=\"mono\">" + escaped_device_host + ".local</span>";
    page += "<strong>Wi-Fi</strong><span>" + htmlEscape(wifi_status) + "</span>";
    if (mdns_url.length() > 0) {
        page += "<strong>Primary URL</strong><span class=\"mono\">" + escaped_mdns_url + "</span>";
    }
    if (direct_url.length() > 0) {
        page += "<strong>Alternative URL</strong><span class=\"mono\">" + escaped_direct_url + "</span>";
    }
    if (setup_url.length() > 0) {
        page += "<strong>Setup Portal</strong><span class=\"mono\">" + escaped_setup_url + "</span>";
    }
    page += "<strong>HomeKit</strong><span><a class=\"status-link\" href=\"/?section=homekit\">";
    page += homekit_is_paired ? String("Paired") : String("Not paired yet");
    page += "</a></span>";
    page += "</div></div>";

    page += "<details class=\"card section-card\"";
    if (open_homekit) {
        page += " open";
    }
    page += F("><summary>Apple Home</summary><div class=\"section-body\">");
    if (homekit_is_paired) {
        page += F("<p>The accessory is already paired to Apple Home. Remove it from the Home app if you want to pair again.</p>");
    } else {
        page += F("<p>Open the Home app and scan this QR code, or enter the pairing code manually.</p>");
        page += F("<div class=\"qr\">");
        page += "<img src=\"/homekit/qr.svg\" alt=\"HomeKit QR code\">";
        page += "<div><p><strong>Pairing code</strong><br><span class=\"mono pairing-code\">" + htmlEscape(pairing_code) + "</span></p>";
        page += "<p><strong>Setup payload</strong><br><span class=\"mono\">" + htmlEscape(pairing_payload) + "</span></p></div>";
        page += F("</div>");
    }
    page += F("</div></details>");

    page += "<details class=\"card section-card\"";
    if (open_device) {
        page += " open";
    }
    page += F("><summary>Device</summary><div class=\"section-body\">"
              "<p>Change the local device label and the mDNS hostname. A reboot is required so Bonjour and the web UI stay consistent.</p>"
              "<form method=\"POST\" action=\"/device/save\">"
              "<input type=\"hidden\" name=\"section\" value=\"device\">"
              "<label for=\"device_label\">Device label</label>");
    page += "<input id=\"device_label\" name=\"device_label\" value=\"" + escaped_device_label + "\" maxlength=\"48\">";
    page += F("<label for=\"device_host\">Hostname</label>");
    page += "<input id=\"device_host\" name=\"device_host\" value=\"" + escaped_device_host + "\" maxlength=\"48\" spellcheck=\"false\">";
    page += F("<p class=\"subtle\">Allowed characters: lowercase letters, numbers and hyphens. If you install multiple devices, give each one a distinct hostname.</p>"
              "<div class=\"actions\"><button class=\"primary\" type=\"submit\">Save and reboot</button></div>"
              "</form></div></details>");

    page += "<details class=\"card section-card\"";
    if (open_wifi) {
        page += " open";
    }
    page += F("><summary>Wi-Fi</summary><div class=\"section-body\">"
              "<p>Update the stored Wi-Fi credentials or reset the device back into setup mode.</p>"
              "<form method=\"POST\" action=\"/wifi/save\">"
              "<input type=\"hidden\" name=\"section\" value=\"wifi\">"
              "<label for=\"wifi_ssid\">Wi-Fi network</label>");
    page += "<input id=\"wifi_ssid\" name=\"ssid\" value=\"" + escaped_stored_ssid + "\" maxlength=\"32\" spellcheck=\"false\" autocapitalize=\"none\" autocomplete=\"username\">";
    page += F("<label for=\"wifi_pwd\">Wi-Fi password</label>"
              "<input id=\"wifi_pwd\" name=\"pwd\" type=\"password\" value=\"\" maxlength=\"64\" placeholder=\"Enter a new password only if it changed\" autocomplete=\"current-password\" autocapitalize=\"none\" spellcheck=\"false\">"
              "<p class=\"subtle\">Saving Wi-Fi restarts the device. Leaving the password empty keeps the current password for the same SSID, or uses an open network for a new SSID.</p>"
              "<div class=\"actions\"><button class=\"primary\" type=\"submit\">Save Wi-Fi and reboot</button></div>"
              "</form>"
              "<form method=\"POST\" action=\"/wifi/reset\" class=\"actions\">"
              "<input type=\"hidden\" name=\"section\" value=\"wifi\">"
              "<button class=\"danger\" type=\"submit\">Reset Wi-Fi and enter setup mode</button>"
              "</form></div></details>");

    page += "<details class=\"card section-card\"";
    if (open_display) {
        page += " open";
    }
    page += F("><summary>Appearance &amp; Display</summary><div class=\"section-body\">"
              "<form method=\"POST\" action=\"/settings/display\">"
              "<input type=\"hidden\" name=\"section\" value=\"display\">"
              "<input type=\"hidden\" name=\"color_idx\" id=\"color_idx\" value=\"\">"
              "<div class=\"row\"><div><label for=\"brightness_scale\">Display Brightness</label>"
              "<div class=\"stepper\">"
              "<button class=\"secondary step-btn\" type=\"button\" onclick=\"adjustBrightness(-1)\" aria-label=\"Dunkler\">&#8722;</button>");
    page += "<input id=\"brightness_scale\" name=\"brightness_scale\" type=\"number\" min=\"0\" max=\"" + String(kWebBrightnessScaleMax) +
            "\" step=\"1\" value=\"" + String(brightness_scale_value) + "\">";
    page += F("<button class=\"secondary step-btn\" type=\"button\" onclick=\"adjustBrightness(1)\" aria-label=\"Heller\">+</button>"
              "</div>");
    page += F("<p class=\"subtle\">Use a simple scale from 0 (darkest) to 10 (brightest).</p>"
              "</div><div><label for=\"color_picker\">Switch ON Color</label>");
    page += "<div class=\"color-picker-wrap\"><input class=\"color-input\" id=\"color_picker\" name=\"color_picker\" type=\"color\" value=\"" + active_color_css + "\">";
    page += F("<span class=\"color-picker-plus\" aria-hidden=\"true\">+</span></div>"
              "<p class=\"subtle\">Tap the color field or choose a quick preset.</p>"
              "<div class=\"swatches\">");
    for (int i = 0; i < kNumColors; ++i) {
        const String preset_color_css = colorHexCss(kColorHexOptions[i]);
        page += "<button class=\"swatch";
        if (kColorHexOptions[i] == active_switch_color_hex) {
            page += " active";
        }
        page += "\" type=\"button\" title=\"" + htmlEscape(kColorLabels[i]) + "\" data-preset-index=\"" + String(i) + "\"";
        page += " data-color=\"" + preset_color_css + "\"";
        page += " style=\"background:" + preset_color_css + ";\"";
        page += " onclick=\"setPresetColor('" + preset_color_css + "', " + String(i) + ")\"></button>";
    }
    page += F("</div><label for=\"color_hex\">Hex Color</label>");
    page += "<input id=\"color_hex\" name=\"color_hex\" value=\"" + active_color_css + "\" maxlength=\"7\" spellcheck=\"false\">";
    page += F("<p class=\"subtle\">Enter a value like #68724D.</p></div></div>");
    page += F("<div class=\"checkboxes\">");
    page += "<div class=\"toggle-item\"><label class=\"checkbox-label\"><input type=\"checkbox\" name=\"screen2_auto_off\"";
    if (!screen2_bl_always_on) {
        page += " checked";
    }
    page += ">Allow Screen 2 to turn off after inactivity</label>";
    page += F("<p class=\"subtle\">Lets Screen 2 go dark after a short idle time, so the display does not keep glowing in dark rooms, especially on non-OLED panels. When disabled, Screen 2 stays lit.</p></div>");
    page += "<div class=\"toggle-item\"><label class=\"checkbox-label\"><input type=\"checkbox\" name=\"px_shift\"";
    if (oled_pixel_shift_enabled) {
        page += " checked";
    }
    page += ">Pixel Shift</label>";
    page += F("<p class=\"subtle\">Moves the UI by a few pixels every so often. This helps reduce uneven OLED aging and image retention on static screens.</p></div>");
    page += "<div class=\"toggle-item\"><label class=\"checkbox-label\"><input type=\"checkbox\" name=\"clock_btn\"";
    if (clock_button_screen_enabled) {
        page += " checked";
    }
    page += ">Clock Button Screen</label>";
    page += F("<p class=\"subtle\">Shows the separate full-screen clock button page below the large single-button screen.</p></div>");
    page += "<div class=\"toggle-item\"><label class=\"checkbox-label\"><input type=\"checkbox\" name=\"clock_sv\"";
    if (oled_clock_saver_enabled) {
        page += " checked";
    }
    page += ">Idle Clock Saver</label>";
    page += F("<p class=\"subtle\">After inactivity, the interface switches to the analog clock screensaver. This is independent from the clock button page.</p></div>");
    page += F("<div class=\"toggle-item\"><label for=\"clock_idle_s\">Standby time</label>");
    page += "<input id=\"clock_idle_s\" name=\"clock_idle_s\" type=\"number\" min=\"10\" max=\"3600\" step=\"5\" value=\"" + String(clock_saver_idle_ms / 1000UL) + "\">";
    page += F("<p class=\"subtle\">Seconds of inactivity before the idle clock saver appears.</p></div>");
    page += F("</div><div class=\"actions\"><button class=\"primary\" type=\"submit\">Save display settings</button></div></form></div></details>");

    page += "<details class=\"card section-card\"";
    if (open_time) {
        page += " open";
    }
    page += F("><summary>Time</summary><div class=\"section-body\">"
              "<form method=\"POST\" action=\"/settings/time\">"
              "<input type=\"hidden\" name=\"section\" value=\"time\">"
              "<label for=\"tz_idx\">Timezone</label>"
              "<select id=\"tz_idx\" name=\"tz_idx\">");
    for (int i = 0; i < kNumTimezones; ++i) {
        page += "<option value=\"" + String(i) + "\"";
        if (i == timezone_index) {
            page += " selected";
        }
        page += ">" + htmlEscape(kTimezoneLabels[i]) + "</option>";
    }
    page += F("</select><div class=\"actions\"><button class=\"primary\" type=\"submit\">Save timezone</button></div></form></div></details>");

    page += "<details class=\"card section-card\"";
    if (open_weather) {
        page += " open";
    }
    page += F(" id=\"weather-section\"><summary>Weather</summary><div class=\"section-body\">");
    if (weather_location_set) {
        page += F("<form method=\"POST\" action=\"/weather/save#weather-section\">"
                  "<input type=\"hidden\" name=\"section\" value=\"weather\">"
                  "<input type=\"hidden\" name=\"weather_visibility\" value=\"1\">");
        page += "<input type=\"hidden\" name=\"name\" value=\"" + escaped_label + "\">";
        page += "<input type=\"hidden\" name=\"latitude\" value=\"" + String(weather_custom_latitude, 6) + "\">";
        page += "<input type=\"hidden\" name=\"longitude\" value=\"" + String(weather_custom_longitude, 6) + "\">";
        page += F("<div class=\"toggle-item\"><label class=\"checkbox-label\"><input type=\"checkbox\" name=\"weather_enabled\"");
        if (weather_enabled) {
            page += " checked";
        }
        page += F(">Show weather on clock views</label>"
                  "<p class=\"subtle\">Applies to both the idle clock saver and the clock button screen. Setting a location turns this on automatically.</p></div>");
        page += F("<div class=\"actions\"><button class=\"secondary\" type=\"submit\">Save weather visibility</button></div>");
        page += F("</form>");
    }
    page += "<p><strong>Weather:</strong> " + String(weather_enabled ? "Enabled" : "Disabled") + "<br>";
    page += "<strong>Current source:</strong> " + String(weather_location_set ? escaped_label : "No location set") + "<br>";
    if (weather_location_set) {
        page += "<strong>Coordinates:</strong> <span class=\"mono\">" + String(weather_custom_latitude, 6) + ", " + String(weather_custom_longitude, 6) + "</span><br>";
    }
    page += "<strong>Current weather:</strong> " + weather_status + "</p>";
    page += F("<form method=\"GET\" action=\"/weather/search#weather-section\">"
              "<input type=\"hidden\" name=\"section\" value=\"weather\">"
              "<label for=\"query\">Find city or village</label>");
    page += "<div class=\"location-search\"><input id=\"query\" name=\"query\" value=\"" + escaped_weather_search_value + "\" placeholder=\"e.g. Berlin, Tegernsee, Winterberg\">";
    page += F("<button class=\"pin-btn\" type=\"button\" title=\"Use current browser location\" aria-label=\"Use current browser location\" onclick=\"useBrowserLocation(true)\">"
              "<svg viewBox=\"0 0 384 512\" aria-hidden=\"true\" focusable=\"false\"><path fill=\"currentColor\" d=\"M192 0C86 0 0 86 0 192c0 77 27 99 172 310 10 14 30 14 40 0 145-211 172-233 172-310C384 86 298 0 192 0zm0 272a80 80 0 1 1 0-160 80 80 0 0 1 0 160z\"/></svg>"
              "</button></div><div id=\"location_permission_hint\" class=\"weather-location-hint\"></div>");
    page += F("<div class=\"actions\"><button class=\"primary\" type=\"submit\">Search place</button></div></form>");
    page += F("<form id=\"browser_location_form\" class=\"hidden-form\" method=\"POST\" action=\"/weather/save#weather-section\">"
              "<input type=\"hidden\" name=\"section\" value=\"weather\">"
              "<input type=\"hidden\" id=\"browser_location_name\" name=\"name\" value=\"Browser location\">"
              "<input type=\"hidden\" id=\"browser_latitude\" name=\"latitude\" value=\"\">"
              "<input type=\"hidden\" id=\"browser_longitude\" name=\"longitude\" value=\"\">"
              "</form>"
              "<p class=\"subtle\">Search uses Open-Meteo geocoding and needs an active Wi-Fi internet connection. Tap the pin to use this browser's current location.</p>");

    if (search_message.length() > 0) {
        page += "<div class=\"msg";
        if (search_is_error) {
            page += " error";
        }
        page += "\">" + escaped_search_message + "</div>";
    }

    if (search_results && search_result_count > 0) {
        page += "<div class=\"search-results\">";
        for (int i = 0; i < search_result_count; ++i) {
            if (!search_results[i].valid) {
                continue;
            }

            const String display_name = weatherSearchDisplayName(search_results[i]);
            const String escaped_display_name = htmlEscape(display_name);
            const String escaped_result_name = htmlEscape(display_name);

            page += F("<form method=\"POST\" action=\"/weather/save#weather-section\" class=\"result-form\">");
            page += F("<input type=\"hidden\" name=\"section\" value=\"weather\">");
            page += "<input type=\"hidden\" name=\"name\" value=\"" + escaped_result_name + "\">";
            page += "<input type=\"hidden\" name=\"latitude\" value=\"" + String(search_results[i].latitude, 6) + "\">";
            page += "<input type=\"hidden\" name=\"longitude\" value=\"" + String(search_results[i].longitude, 6) + "\">";
            page += F("<button class=\"result-btn\" type=\"submit\">");
            page += "<span class=\"result-title\">" + escaped_display_name + "</span>";
            page += "<span class=\"result-meta mono\">" + String(search_results[i].latitude, 6) + ", " +
                    String(search_results[i].longitude, 6) + "</span>";
            page += F("</button></form>");
        }
        page += F("</div>");
    }

    page += F("<details class=\"manual-location\"><summary>Manual coordinates</summary><div class=\"manual-body\">"
              "<form method=\"POST\" action=\"/weather/save#weather-section\">"
              "<input type=\"hidden\" name=\"section\" value=\"weather\">"
              "<p class=\"subtle\">Advanced: enter exact coordinates manually. Saving coordinates turns weather on automatically.</p>"
              "<label for=\"name\">Location name</label>");
    page += "<input id=\"name\" name=\"name\" value=\"" + escaped_name + "\" placeholder=\"e.g. Munich Home\">";
    page += F("<div class=\"row\"><div><label for=\"latitude\">Latitude</label>");
    page += "<input id=\"latitude\" name=\"latitude\" value=\"" + String(weather_location_set ? String(weather_custom_latitude, 6) : String()) + "\" inputmode=\"decimal\">";
    page += F("</div><div><label for=\"longitude\">Longitude</label>");
    page += "<input id=\"longitude\" name=\"longitude\" value=\"" + String(weather_location_set ? String(weather_custom_longitude, 6) : String()) + "\" inputmode=\"decimal\">";
    page += F("</div></div><div class=\"actions\">"
              "<button class=\"primary\" type=\"submit\">Save location</button>"
              "<button class=\"secondary\" type=\"button\" onclick=\"useBrowserLocation(false)\">Fill with browser location</button>"
              "</div></form></div></details>");

    page += F("<form method=\"POST\" action=\"/weather/reset#weather-section\" class=\"actions\">"
              "<input type=\"hidden\" name=\"section\" value=\"weather\">"
              "<button class=\"danger\" type=\"submit\">Clear weather location</button>"
              "</form></div></details>");

    page += F("<script>"
              "function normalizeHexColor(value){"
              "let hex=(value||'').trim().toUpperCase();"
              "if(!hex.startsWith('#')){hex='#'+hex;}"
              "return /^#[0-9A-F]{6}$/.test(hex)?hex:'';"
              "}"
              "function updatePresetSelection(hex,presetIndex){"
              "document.querySelectorAll('.swatch').forEach(btn=>btn.classList.remove('active'));"
              "if(typeof presetIndex==='number'&&presetIndex>=0){"
              "const preset=document.querySelector('.swatch[data-preset-index=\"'+presetIndex+'\"]');"
              "if(preset){preset.classList.add('active');}"
              "return;"
              "}"
              "const normalized=normalizeHexColor(hex);"
              "if(!normalized){return;}"
              "document.querySelectorAll('.swatch').forEach(btn=>{"
              "if(normalizeHexColor(btn.dataset.color)===normalized){btn.classList.add('active');}"
              "});"
              "}"
              "function setPresetColor(color,index){"
              "document.getElementById('color_picker').value=color;"
              "document.getElementById('color_hex').value=color.toUpperCase();"
              "document.getElementById('color_idx').value=String(index);"
              "updatePresetSelection(color,index);"
              "}"
              "function adjustBrightness(delta){"
              "const input=document.getElementById('brightness_scale');"
              "if(!input){return;}"
              "const min=parseInt(input.min||'0',10);"
              "const max=parseInt(input.max||'10',10);"
              "const current=parseInt(input.value||'0',10);"
              "const next=Math.min(max,Math.max(min,(isNaN(current)?min:current)+delta));"
              "input.value=String(next);"
              "}"
              "function showLocationHint(message){"
              "const hint=document.getElementById('location_permission_hint');"
              "if(hint){hint.textContent=message;hint.classList.add('show');}"
              "else{alert(message);}"
              "}"
              "function useBrowserLocation(saveImmediately){"
              "if(window.isSecureContext===false){"
              "showLocationHint('Browser location is blocked because this page is opened over HTTP. Safari only asks for location permission on secure pages. Please search for your place manually.');"
              "return;"
              "}"
              "if(!navigator.geolocation){showLocationHint('Geolocation is not supported by this browser. Please search for your place manually.');return;}"
              "navigator.geolocation.getCurrentPosition(function(pos){"
              "const hint=document.getElementById('location_permission_hint');"
              "if(hint){hint.classList.remove('show');}"
              "const lat=pos.coords.latitude.toFixed(6);"
              "const lon=pos.coords.longitude.toFixed(6);"
              "if(saveImmediately){"
              "document.getElementById('browser_latitude').value=lat;"
              "document.getElementById('browser_longitude').value=lon;"
              "document.getElementById('browser_location_form').submit();"
              "return;"
              "}"
              "document.getElementById('latitude').value=lat;"
              "document.getElementById('longitude').value=lon;"
              "const name=document.getElementById('name');"
              "if(!name.value){name.value='Browser location';}"
              "},function(err){showLocationHint('Unable to read your location: '+err.message);},{enableHighAccuracy:true,timeout:10000,maximumAge:60000});"
              "}"
              "document.addEventListener('DOMContentLoaded',function(){"
              "const picker=document.getElementById('color_picker');"
              "const hex=document.getElementById('color_hex');"
              "const preset=document.getElementById('color_idx');"
              "if(picker&&hex){"
              "picker.addEventListener('input',function(){"
              "hex.value=picker.value.toUpperCase();"
              "preset.value='';"
              "updatePresetSelection(picker.value,-1);"
              "});"
              "hex.addEventListener('input',function(){"
              "const normalized=normalizeHexColor(hex.value);"
              "preset.value='';"
              "if(normalized){picker.value=normalized;updatePresetSelection(normalized,-1);}"
              "});"
              "}"
              "const params=new URLSearchParams(window.location.search);"
              "const weather=document.getElementById('weather-section');"
              "if(weather&&(window.location.pathname.indexOf('/weather')===0||params.get('section')==='weather'||window.location.hash==='#weather-section')){"
              "setTimeout(function(){weather.scrollIntoView({block:'start'});},50);"
              "}"
              "});"
              "</script></div></body></html>");

    return page;
}

void handleWebHome()
{
    weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("", false, "", nullptr, 0, "", false, defaultAdminSection(weather_config_server.arg("section"))));
}

void handleWeatherConfigRoot()
{
    weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("", false, "", nullptr, 0, "", false, defaultAdminSection(weather_config_server.arg("section"))));
}

void handleWeatherConfigSearch()
{
    String query = weather_config_server.arg("query");
    query.trim();

    if (query.length() < 2) {
        weather_config_server.send(
            400,
            "text/html; charset=utf-8",
            renderAdminPage("", false, query, nullptr, 0, "Please enter at least 2 characters.", true, "weather"));
        return;
    }

    WeatherSearchResult results[kWeatherSearchResultLimit] = {};
    int result_count = 0;
    String error_message;
    const bool success = fetchWeatherSearchResults(query, results, kWeatherSearchResultLimit, result_count, error_message);
    String page_message = error_message;
    if (success) {
        page_message = String(result_count) + ((result_count == 1) ? " matching place found." : " matching places found.");
    }

    weather_config_server.send(
        200,
        "text/html; charset=utf-8",
        renderAdminPage("", false, query, success ? results : nullptr, success ? result_count : 0, page_message, !success, "weather"));
}

void redirectToWeatherSection()
{
    weather_config_server.sendHeader("Location", "/?section=weather#weather-section", true);
    weather_config_server.send(303, "text/plain; charset=utf-8", "Returning to Weather settings...");
}

void handleWeatherConfigSave()
{
    const bool visibility_only = weather_config_server.hasArg("weather_visibility");
    const bool next_weather_enabled = visibility_only ? weather_config_server.hasArg("weather_enabled") : true;
    float latitude = 0.0f;
    float longitude = 0.0f;
    const String latitude_arg = weather_config_server.arg("latitude");
    const String longitude_arg = weather_config_server.arg("longitude");

    if (!parseFloatParameter(latitude_arg, latitude) || !parseFloatParameter(longitude_arg, longitude) ||
        !isValidWeatherCoordinates(latitude, longitude)) {
        if (visibility_only && !next_weather_enabled) {
            applyWeatherEnabledSetting(false, true);
            weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("Weather disabled.", false, "", nullptr, 0, "", false, "weather"));
            return;
        }
        if (visibility_only) {
            weather_config_server.send(400, "text/html; charset=utf-8", renderAdminPage("Set a weather location before enabling weather.", true, "", nullptr, 0, "", false, "weather"));
            return;
        }
        weather_config_server.send(400, "text/html; charset=utf-8", renderAdminPage("Please enter valid latitude and longitude values.", true, "", nullptr, 0, "", false, "weather"));
        return;
    }

    String location_name = weather_config_server.arg("name");
    location_name.trim();
    if (location_name.length() == 0) {
        location_name = "Custom location";
    }

    applyWeatherEnabledSetting(next_weather_enabled, false);
    saveCustomWeatherLocation(location_name, latitude, longitude, false);
    saveSettingsToNVS();
    redirectToWeatherSection();
}

void handleWeatherConfigReset()
{
    resetCustomWeatherLocation(false);
    applyWeatherEnabledSetting(false, false);
    saveSettingsToNVS();
    redirectToWeatherSection();
}

void handleDisplaySettingsSave()
{
    uint32_t next_color_hex = active_switch_color_hex;
    const String color_hex_arg = weather_config_server.arg("color_hex");
    const String color_picker_arg = weather_config_server.arg("color_picker");

    if (color_hex_arg.length() > 0) {
        if (!parseHexColor(color_hex_arg, next_color_hex)) {
            weather_config_server.send(
                400,
                "text/html; charset=utf-8",
                renderAdminPage("Please enter a valid hex color like #68724D.", true, "", nullptr, 0, "", false, "display"));
            return;
        }
    } else if (!parseHexColor(color_picker_arg, next_color_hex)) {
        int color_index = weather_config_server.arg("color_idx").toInt();
        if (color_index < 0 || color_index >= kNumColors) {
            color_index = 0;
            for (int i = 0; i < kNumColors; ++i) {
                if (kColorHexOptions[i] == active_switch_color_hex) {
                    color_index = i;
                    break;
                }
            }
        }
        next_color_hex = kColorHexOptions[color_index];
    }

    active_switch_color_hex = next_color_hex;
    applyNewColor(lv_color_hex(active_switch_color_hex));
    const int brightness_scale = weather_config_server.hasArg("brightness_scale")
                                     ? weather_config_server.arg("brightness_scale").toInt()
                                     : brightnessScaleForWeb(global_brightness);
    applyBrightnessSetting(brightnessLevelFromWebScale(brightness_scale), false);
    applyScreen2BacklightSetting(!weather_config_server.hasArg("screen2_auto_off"), false);
    applyPixelShiftSetting(weather_config_server.hasArg("px_shift"), false);
    applyClockButtonSetting(weather_config_server.hasArg("clock_btn"), false);
    applyClockSaverSetting(weather_config_server.hasArg("clock_sv"), false);
    long clock_idle_seconds_arg = weather_config_server.hasArg("clock_idle_s")
                                      ? weather_config_server.arg("clock_idle_s").toInt()
                                      : static_cast<long>(clock_saver_idle_ms / 1000UL);
    if (clock_idle_seconds_arg < 0) {
        clock_idle_seconds_arg = 0;
    }
    const uint32_t clock_idle_seconds = static_cast<uint32_t>(clock_idle_seconds_arg);
    applyClockSaverIdleSetting(clock_idle_seconds * 1000UL, false);
    saveSettingsToNVS();

    weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("Display settings saved.", false, "", nullptr, 0, "", false, "display"));
}

void handleTimezoneSave()
{
    const int requested_timezone = weather_config_server.arg("tz_idx").toInt();
    if (requested_timezone < 0 || requested_timezone >= kNumTimezones) {
        weather_config_server.send(400, "text/html; charset=utf-8", renderAdminPage("Please choose a valid timezone.", true, "", nullptr, 0, "", false, "time"));
        return;
    }

    applyTimezoneSettingIndex(requested_timezone, true);
    weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("Timezone saved.", false, "", nullptr, 0, "", false, "time"));
}

void handleDeviceConfigSave()
{
    String next_label = weather_config_server.arg("device_label");
    next_label.trim();
    if (next_label.length() == 0) {
        next_label = defaultDeviceLabel();
    }

    device_label = next_label;
    device_host_name = sanitizeHostName(weather_config_server.arg("device_host"));
    saveDeviceConfigToNVS();
    scheduleReboot();

    weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("Device settings saved. Rebooting now.", false, "", nullptr, 0, "", false, "device"));
}

void handleWifiConfigSave()
{
    String ssid = weather_config_server.arg("ssid");
    ssid.trim();
    if (ssid.length() == 0) {
        weather_config_server.send(400, "text/html; charset=utf-8", renderAdminPage("Please enter a Wi-Fi network name.", true, "", nullptr, 0, "", false, "wifi"));
        return;
    }

    String password = weather_config_server.arg("pwd");
    StoredWifiCredentials existing_credentials = {};
    const bool has_existing_credentials = readStoredWifiCredentials(existing_credentials);
    if (password.length() == 0 && has_existing_credentials && ssid == String(existing_credentials.ssid)) {
        password = String(existing_credentials.pwd);
    }

    StoredWifiCredentials credentials = {};
    snprintf(credentials.ssid, sizeof(credentials.ssid), "%s", ssid.c_str());
    snprintf(credentials.pwd, sizeof(credentials.pwd), "%s", password.c_str());

    if (!saveStoredWifiCredentials(credentials)) {
        weather_config_server.send(500, "text/html; charset=utf-8", renderAdminPage("Unable to save Wi-Fi credentials.", true, "", nullptr, 0, "", false, "wifi"));
        return;
    }

    scheduleReboot();
    weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("Wi-Fi saved. Rebooting now.", false, "", nullptr, 0, "", false, "wifi"));
}

void handleWifiReset()
{
    eraseStoredWifiCredentials();
    scheduleReboot();
    weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("Wi-Fi reset. Rebooting into setup mode.", false, "", nullptr, 0, "", false, "wifi"));
}

void handleHomeKitQrSvg()
{
    const String svg = renderQrSvg(homeKitSetupPayload());
    if (svg.length() == 0) {
        weather_config_server.send(500, "text/plain; charset=utf-8", "Unable to render QR code.");
        return;
    }

    weather_config_server.send(200, "image/svg+xml", svg);
}

void startWeatherConfigServer()
{
    if (weather_config_server_routes_registered) {
        return;
    }

    weather_config_server.on("/", HTTP_GET, handleWebHome);
    weather_config_server.on("/weather", HTTP_GET, handleWeatherConfigRoot);
    weather_config_server.on("/weather/search", HTTP_GET, handleWeatherConfigSearch);
    weather_config_server.on("/weather/save", HTTP_POST, handleWeatherConfigSave);
    weather_config_server.on("/weather/reset", HTTP_POST, handleWeatherConfigReset);
    weather_config_server.on("/settings/display", HTTP_POST, handleDisplaySettingsSave);
    weather_config_server.on("/settings/time", HTTP_POST, handleTimezoneSave);
    weather_config_server.on("/device/save", HTTP_POST, handleDeviceConfigSave);
    weather_config_server.on("/wifi/save", HTTP_POST, handleWifiConfigSave);
    weather_config_server.on("/wifi/reset", HTTP_POST, handleWifiReset);
    weather_config_server.on("/homekit/qr.svg", HTTP_GET, handleHomeKitQrSvg);
    weather_config_server_routes_registered = true;
}

void ensureWeatherConfigServer()
{
    if (weather_config_server_running || !weather_config_server_routes_registered || provisioning_mode_active) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    const IPAddress station_ip = WiFi.localIP();
    const bool can_start = station_ip[0] != 0 || station_ip[1] != 0 || station_ip[2] != 0 || station_ip[3] != 0;
    if (!can_start) {
        return;
    }

    weather_config_server.begin();
    weather_config_server_running = true;
    Serial.println("Admin web UI started");
}

void handleProvisioningRoot()
{
    provisioning_server.send(200, "text/html; charset=utf-8", renderProvisioningPage());
}

void handleProvisioningWifiScan()
{
    if (!provisioning_mode_active) {
        provisioning_server.send(503, "application/json; charset=utf-8", "[]");
        return;
    }

    WiFi.scanDelete();
    const int network_count = WiFi.scanNetworks(false, true);
    String json = "[";
    String unique_ssids[24];
    int unique_count = 0;

    for (int i = 0; i < network_count && unique_count < 24; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) {
            continue;
        }

        bool duplicate = false;
        for (int j = 0; j < unique_count; ++j) {
            if (unique_ssids[j] == ssid) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        unique_ssids[unique_count++] = ssid;
        if (json.length() > 1) {
            json += ",";
        }
        json += "{\"ssid\":\"";
        json += jsonEscape(ssid);
        json += "\",\"rssi\":";
        json += String(WiFi.RSSI(i));
        json += ",\"secure\":";
        json += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true";
        json += "}";
    }
    json += "]";

    provisioning_server.send(200, "application/json; charset=utf-8", json);
}

void handleProvisioningWifiSave()
{
    String ssid = provisioning_server.arg("ssid");
    ssid.trim();
    if (ssid.length() == 0) {
        provisioning_server.send(400, "text/html; charset=utf-8", renderProvisioningPage("Please choose a Wi-Fi network.", true));
        return;
    }

    String next_label = provisioning_server.arg("device_label");
    next_label.trim();
    if (next_label.length() == 0) {
        next_label = defaultDeviceLabel();
    }

    device_label = next_label;
    device_host_name = sanitizeHostName(provisioning_server.arg("device_host"));
    saveDeviceConfigToNVS();

    StoredWifiCredentials credentials = {};
    const String password = provisioning_server.arg("pwd");
    snprintf(credentials.ssid, sizeof(credentials.ssid), "%s", ssid.c_str());
    snprintf(credentials.pwd, sizeof(credentials.pwd), "%s", password.c_str());

    if (!saveStoredWifiCredentials(credentials)) {
        provisioning_server.send(500, "text/html; charset=utf-8", renderProvisioningPage("Unable to save Wi-Fi credentials.", true));
        return;
    }

    scheduleReboot();
    provisioning_server.send(200, "text/html; charset=utf-8", renderProvisioningPage("Wi-Fi saved. The device is rebooting now."));
}

void handleProvisioningRedirect()
{
    provisioning_server.sendHeader("Location", setupPortalUrl(), true);
    provisioning_server.send(302, "text/plain", "");
}

String renderProvisioningPage(const String &message, bool is_error)
{
    const String escaped_message = htmlEscape(message);
    const String escaped_label = htmlEscape(device_label);
    const String escaped_host = htmlEscape(device_host_name);
    const String escaped_ssid = htmlEscape(setupAccessPointSsid());
    String page;
    page.reserve(9000);
    page += F(
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>HOMEsmthng Setup</title>"
        "<style>"
        ":root{color-scheme:dark;--bg:#0c0f10;--surface:#171b1d;--surface-2:#111517;--border:#2a2f33;--border-strong:#40484d;--text:#f4f4f4;--muted:#c7cccf;--subtle:#98a1a6;--input:#0f1315;--primary:#68724D;--secondary:#273038;--msg:#1f2d24;--msg-text:#d7f2df;--error:#3a2020;--error-text:#ffd6d6;--shadow:0 18px 50px rgba(0,0,0,.22);}"
        "@media(prefers-color-scheme:light){:root{color-scheme:light;--bg:#f5f2eb;--surface:#fffdf8;--surface-2:#f4f0e7;--border:#ded8cc;--border-strong:#bdb5a6;--text:#171b1d;--muted:#4b5257;--subtle:#737b80;--input:#ffffff;--primary:#68724D;--secondary:#e8e1d3;--msg:#e4efdc;--msg-text:#28361f;--error:#f2d7d4;--error-text:#5b1e18;--shadow:0 18px 50px rgba(74,63,44,.14);}}"
        "*{transition:background-color .18s ease,border-color .18s ease,color .18s ease,box-shadow .18s ease,transform .18s ease;}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:radial-gradient(circle at top left,color-mix(in srgb,var(--primary) 18%,transparent),transparent 34rem),var(--bg);color:var(--text);margin:0;padding:24px;}"
        ".wrap{max-width:720px;margin:0 auto;}"
        ".card{background:var(--surface);border:1px solid var(--border);border-radius:18px;padding:20px;margin-bottom:18px;box-shadow:0 0 0 rgba(0,0,0,0);}"
        ".card:hover{border-color:var(--border-strong);box-shadow:var(--shadow);transform:translateY(-1px);}"
        "h1,h2{margin:0 0 12px 0;}p{color:var(--muted);line-height:1.45;}"
        "label{display:block;margin:14px 0 6px 0;font-weight:600;}"
        "input{width:100%;box-sizing:border-box;border-radius:12px;border:1px solid var(--border-strong);background:var(--input);color:var(--text);padding:14px;font-size:16px;}"
        "input:hover{border-color:var(--primary);}input:focus,button:focus-visible{outline:2px solid color-mix(in srgb,var(--primary) 70%,white);outline-offset:2px;}"
        ".actions{display:flex;gap:12px;flex-wrap:wrap;margin-top:18px;}"
        "button{border:0;border-radius:999px;padding:12px 18px;font-size:15px;font-weight:600;cursor:pointer;}"
        "button:hover{transform:translateY(-1px);filter:brightness(1.08);}button:active{transform:translateY(0) scale(.98);}"
        ".primary{background:var(--primary);color:#fff;}.secondary{background:var(--secondary);color:var(--text);}"
        ".msg{margin-bottom:16px;padding:12px 14px;border-radius:12px;background:var(--msg);color:var(--msg-text);}"
        ".msg.error{background:var(--error);color:var(--error-text);}"
        ".subtle{color:var(--subtle);font-size:14px;}"
        ".mono{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:14px;color:var(--muted);overflow-wrap:anywhere;word-break:break-word;}"
        ".scan-results{display:flex;flex-direction:column;gap:8px;margin-top:10px;}"
        ".scan-item{display:flex;justify-content:space-between;gap:12px;background:var(--surface-2);border:1px solid var(--border);border-radius:14px;padding:12px 14px;color:var(--text);}"
        ".scan-item:hover{border-color:var(--primary);box-shadow:0 10px 24px color-mix(in srgb,var(--primary) 16%,transparent);transform:translateY(-1px);}"
        ".scan-panel{margin:8px 0 14px 0;border:1px solid var(--border);border-radius:14px;background:var(--surface-2);overflow:hidden;}"
        ".scan-panel summary{list-style:none;cursor:pointer;padding:12px 14px;color:var(--subtle);font-size:14px;font-weight:600;}"
        ".scan-panel summary:hover{color:var(--text);background:color-mix(in srgb,var(--primary) 10%,transparent);}"
        ".scan-panel summary::-webkit-details-marker{display:none;}"
        ".scan-panel summary::after{content:'›';float:right;transition:transform .18s ease;}"
        ".scan-panel[open] summary::after{transform:rotate(90deg);}"
        ".scan-panel[open] summary{border-bottom:1px solid var(--border);}"
        "@media(max-width:520px){body{padding:14px;}.card{padding:16px;}.scan-item{flex-direction:column;}}"
        "</style></head><body><div class=\"wrap\">"
        "<div class=\"card\"><h1>HOMEsmthng Setup</h1>"
        "<p>Connect this device to your home Wi-Fi. After saving the network, the device restarts and the admin UI moves to the local network.</p>"
        "<p><strong>Setup Wi-Fi</strong><br><span class=\"mono\">");
    page += escaped_ssid;
    page += F("</span><br><strong>Portal</strong><br><span class=\"mono\">http://192.168.4.1</span></p></div>");

    if (message.length() > 0) {
        page += "<div class=\"msg";
        if (is_error) {
            page += " error";
        }
        page += "\">" + escaped_message + "</div>";
    }

    page += F("<div id=\"wifi-setup\" class=\"card\"><h2>Provision Device</h2>"
              "<form method=\"POST\" action=\"/wifi/save\">"
              "<label for=\"device_label\">Device label</label>");
    page += "<input id=\"device_label\" name=\"device_label\" value=\"" + escaped_label + "\" maxlength=\"48\">";
    page += F("<label for=\"device_host\">Hostname</label>");
    page += "<input id=\"device_host\" name=\"device_host\" value=\"" + escaped_host + "\" maxlength=\"48\" spellcheck=\"false\">";
    page += F("<p class=\"subtle\">Each device should have its own hostname, for example <span class=\"mono\">homesmthng-abcd</span>.</p>");
    page += F(
        "<label for=\"ssid\">Wi-Fi network</label>"
              "<input id=\"ssid\" name=\"ssid\" list=\"ssid-list\" placeholder=\"Choose or type a Wi-Fi network\" maxlength=\"32\" required spellcheck=\"false\" autocapitalize=\"none\" autocomplete=\"username\">"
              "<datalist id=\"ssid-list\"></datalist>"
              "<details id=\"scan-panel\" class=\"scan-panel\"><summary id=\"scan-summary\">Scan networks</summary><div id=\"scan-results\" class=\"scan-results\"></div></details>"
              "<label for=\"pwd\">Wi-Fi password</label>"
              "<input id=\"pwd\" name=\"pwd\" type=\"password\" maxlength=\"64\" placeholder=\"Leave empty only for open networks\" autocomplete=\"current-password\" autocapitalize=\"none\" spellcheck=\"false\">"
              "<div class=\"actions\">"
              "<button class=\"primary\" type=\"submit\">Save Wi-Fi and reboot</button>"
              "</div></form>"
              "</div>"
              "<script>"
              "let scanRunning=false;"
              "async function refreshNetworks(){"
              "if(scanRunning){return;}"
              "scanRunning=true;"
              "const list=document.getElementById('ssid-list');"
              "const panel=document.getElementById('scan-panel');"
              "const summary=document.getElementById('scan-summary');"
              "const results=document.getElementById('scan-results');"
              "summary.textContent='Scanning networks...';"
              "list.innerHTML='';results.innerHTML='<div class=\"scan-item\"><span>Scanning Wi-Fi networks...</span></div>';"
              "try{const response=await fetch('/wifi/scan',{cache:'no-store'});"
              "const items=await response.json();"
              "results.innerHTML='';"
              "summary.textContent=items.length?('Scanned networks ('+items.length+')'):'Scanned networks';"
              "if(!items.length){results.innerHTML='<div class=\"scan-item\"><span>No Wi-Fi networks found.</span></div>';return;}"
              "items.forEach(item=>{"
              "const option=document.createElement('option');option.value=item.ssid;list.appendChild(option);"
              "const row=document.createElement('button');row.type='button';row.className='scan-item';"
              "row.innerHTML='<span>'+item.ssid+'</span><span>'+item.rssi+' dBm'+(item.secure?' · secured':' · open')+'</span>';"
              "row.onclick=()=>{document.getElementById('ssid').value=item.ssid;panel.open=false;summary.textContent='Selected: '+item.ssid;document.getElementById('pwd').focus();};"
              "results.appendChild(row);"
              "});"
              "}catch(err){summary.textContent='Scan networks';results.innerHTML='<div class=\"scan-item\"><span>Wi-Fi scan failed. Try again.</span></div>';}"
              "finally{scanRunning=false;}}"
              "document.addEventListener('DOMContentLoaded',()=>{const target=document.getElementById('wifi-setup');if(target){setTimeout(()=>target.scrollIntoView({block:'start'}),150);}});"
              "document.addEventListener('DOMContentLoaded',()=>{const panel=document.getElementById('scan-panel');if(panel){panel.addEventListener('toggle',()=>{if(panel.open){refreshNetworks();}});}});"
              "</script></div></body></html>");
    return page;
}

void startProvisioningServer()
{
    if (!provisioning_server_routes_registered) {
        provisioning_server.on("/", HTTP_GET, handleProvisioningRoot);
        provisioning_server.on("/wifi/scan", HTTP_GET, handleProvisioningWifiScan);
        provisioning_server.on("/wifi/save", HTTP_POST, handleProvisioningWifiSave);
        provisioning_server.on("/generate_204", HTTP_GET, handleProvisioningRedirect);
        provisioning_server.on("/gen_204", HTTP_GET, handleProvisioningRedirect);
        provisioning_server.on("/success.txt", HTTP_GET, handleProvisioningRedirect);
        provisioning_server.on("/redirect", HTTP_GET, handleProvisioningRedirect);
        provisioning_server.on("/canonical.html", HTTP_GET, handleProvisioningRedirect);
        provisioning_server.on("/library/test/success.html", HTTP_GET, handleProvisioningRedirect);
        provisioning_server.on("/hotspot-detect.html", HTTP_GET, handleProvisioningRedirect);
        provisioning_server.on("/connecttest.txt", HTTP_GET, handleProvisioningRedirect);
        provisioning_server.on("/ncsi.txt", HTTP_GET, handleProvisioningRedirect);
        provisioning_server.on("/fwlink", HTTP_GET, handleProvisioningRedirect);
        provisioning_server.onNotFound(handleProvisioningRedirect);
        provisioning_server_routes_registered = true;
    }

    if (provisioning_server_running) {
        return;
    }

    const IPAddress ap_ip(192, 168, 4, 1);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(ap_ip, ap_ip, IPAddress(255, 255, 255, 0));
    WiFi.softAP(setupAccessPointSsid().c_str());
    provisioning_dns_server.start(53, "*", ap_ip);
    provisioning_server.begin();
    provisioning_server_running = true;
    provisioning_mode_active = true;
    setWifiSetupInfoVisible(true, true);
    updateWiFiSymbol();
}

void startProvisioningMode()
{
    startProvisioningServer();
}

void logWeatherConfigUrls()
{
    static String last_mdns_url;
    static String last_direct_url;
    static String last_setup_url;

    const String mdns_url = adminMdnsUrl();
    const String direct_url = adminDirectUrl();
    const String setup_url = setupPortalUrl();

    if (mdns_url != last_mdns_url) {
        last_mdns_url = mdns_url;
        if (mdns_url.length() > 0) {
            Serial.printf("Admin UI (mDNS): %s\n", mdns_url.c_str());
        }
    }

    if (direct_url != last_direct_url) {
        last_direct_url = direct_url;
        if (direct_url.length() > 0) {
            Serial.printf("Admin UI (IP): %s\n", direct_url.c_str());
        }
    }

    if (setup_url != last_setup_url) {
        last_setup_url = setup_url;
        if (setup_url.length() > 0) {
            Serial.printf("Setup portal: %s\n", setup_url.c_str());
        }
    }
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
    closeWeatherSetupModal();

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

bool isSetupAccessPointVisible()
{
    return provisioning_mode_active;
}

void setWifiSetupInfoVisible(bool visible, bool force_main_tile)
{
    if (!ui_wifi_setup_msg || !tileview) {
        return;
    }

    if (visible && force_main_tile && tileMain) {
        lv_obj_set_tile(tileview, tileMain, LV_ANIM_OFF);
    }

    if (visible == wifi_setup_info_visible) {
        if (visible) {
            updateWifiSetupMessage();
        }
        return;
    }

    if (visible) {
        updateWifiSetupMessage();
        lv_obj_clear_flag(ui_wifi_setup_msg, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < kNumUiSwitches; ++i) {
            if (ui_switch_buttons[i]) {
                lv_obj_add_flag(ui_switch_buttons[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else {
        lv_obj_add_flag(ui_wifi_setup_msg, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < kNumUiSwitches; ++i) {
            if (ui_switch_buttons[i]) {
                lv_obj_clear_flag(ui_switch_buttons[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    wifi_setup_info_visible = visible;
}

void homeSpanStatusCallback(HS_STATUS status)
{
    switch (status) {
    case HS_PAIRING_NEEDED:
        homekit_is_paired = false;
        updateWiFiSymbol();
        break;
    case HS_PAIRED:
        homekit_is_paired = true;
        updateWiFiSymbol();
        break;
    default:
        break;
    }
}

void updateOledProtection()
{
    static unsigned long last_shift_update = 0;
    static unsigned long last_clock_update = 0;
    static size_t shift_index = 0;
    lv_obj_t *active_tile = tileview ? lv_tileview_get_tile_act(tileview) : nullptr;
    const bool suppress_screensaver = (active_tile == tileMasterClock && isClockSaverEnabled());

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

    if (screensaver_visible && suppress_screensaver) {
        hideScreensaver(false);
    }

    if (isClockSaverEnabled()) {
        if (!screensaver_visible && !suppress_screensaver && inactive_time >= clock_saver_idle_ms) {
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
    if (!ui_wifi_label || !tileview) {
        return;
    }

    const wl_status_t wifiStatus = WiFi.status();
    const bool showSetupInfo = provisioning_mode_active || isPairingOnboardingActive();
    static wl_status_t lastStatus = WL_DISCONNECTED;
    static bool lastSetupInfo = false;

    if (wifiStatus != lastStatus || showSetupInfo != lastSetupInfo) {
        if (wifiStatus == WL_CONNECTED) {
            lv_label_set_text(ui_wifi_label, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(ui_wifi_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else if (showSetupInfo) {
            lv_label_set_text(ui_wifi_label, LV_SYMBOL_SETTINGS);
            lv_obj_set_style_text_color(ui_wifi_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
        } else {
            lv_label_set_text(ui_wifi_label, LV_SYMBOL_WARNING);
            lv_obj_set_style_text_color(ui_wifi_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
        lastStatus = wifiStatus;
        lastSetupInfo = showSetupInfo;
    }

    const lv_obj_t *current_tile = lv_tileview_get_tile_act(tileview);

    if (showSetupInfo) {
        setWifiSetupInfoVisible(true, current_tile != tileMain);
    } else {
        setWifiSetupInfoVisible(false, false);
    }

    updateWeatherSetupButtonLabel();
    updateWeatherSetupModalContent();
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

    if (id == 8) {
        if (ui_big_button) {
            if (state) {
                lv_obj_add_state(ui_big_button, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(ui_big_button, LV_STATE_CHECKED);
            }
        }
        if (ui_master_clock_button) {
            if (state) {
                lv_obj_add_state(ui_master_clock_button, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(ui_master_clock_button, LV_STATE_CHECKED);
            }
        }
        updateMasterClockAppearance();
        if (isMasterClockTileActive()) {
            updateMasterClock(true);
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
    if (ui_brightness_slider) {
        lv_obj_set_style_bg_color(ui_brightness_slider, active_switch_color, LV_PART_INDICATOR);
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
    if (ui_weather_setup_value_label) {
        lv_obj_set_style_text_color(ui_weather_setup_value_label, active_switch_color, 0);
    }
    if (ui_weather_setup_done_button) {
        lv_obj_set_style_bg_color(ui_weather_setup_done_button, active_switch_color, 0);
    }

    applyClockAccentColor(active_switch_color);
    updateMasterClockAppearance();
}

void saveSettingsToNVS()
{
    preferences.begin("homespan", false);
    preferences.putInt("brightness", global_brightness);
    preferences.putUInt("color_hex", active_switch_color_hex);
    preferences.putBool("screen2_bl", screen2_bl_always_on);
    preferences.putBool("px_shift", oled_pixel_shift_enabled);
    preferences.putBool("clock_btn", clock_button_screen_enabled);
    preferences.putBool("clock_sv", oled_clock_saver_enabled);
    preferences.putUInt("clock_idle", clock_saver_idle_ms);
    preferences.putInt("tz_idx", timezone_index);
    preferences.putBool("wx_enabled", weather_enabled);
    preferences.putBool("wx_custom", weather_use_custom_location);
    preferences.putString("wx_name", weather_location_name);
    preferences.putFloat("wx_lat", weather_custom_latitude);
    preferences.putFloat("wx_lon", weather_custom_longitude);
    preferences.end();
}

void loadSettingsFromNVS()
{
    preferences.begin("homespan", true);
    const int default_brightness = brightnessLevelFromWebScale(kDefaultBrightnessScale);
    global_brightness = preferences.getInt("brightness", default_brightness);
    active_switch_color_hex = preferences.getUInt("color_hex", kDefaultColorHex);
    screen2_bl_always_on = preferences.getBool("screen2_bl", true);
    oled_pixel_shift_enabled = preferences.getBool("px_shift", false);
    oled_clock_saver_enabled = preferences.getBool("clock_sv", supportsClockSaver());
    clock_button_screen_enabled = preferences.getBool("clock_btn", supportsClockSaver());
    clock_saver_idle_ms = preferences.getUInt("clock_idle", kClockSaverIdleMs);
    timezone_index = preferences.getInt("tz_idx", kDefaultTimezoneIndex);
    weather_enabled = preferences.getBool("wx_enabled", true);
    weather_use_custom_location = preferences.getBool("wx_custom", false);
    weather_location_name = preferences.getString("wx_name", "");
    weather_custom_latitude = preferences.getFloat("wx_lat", 0.0f);
    weather_custom_longitude = preferences.getFloat("wx_lon", 0.0f);
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
        clock_button_screen_enabled = false;
    }
    clock_saver_idle_ms = constrain(clock_saver_idle_ms, kClockSaverMinIdleMs, kClockSaverMaxIdleMs);
    if (!hasCustomWeatherLocation()) {
        weather_use_custom_location = false;
        weather_location_name = "";
        weather_has_data = false;
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
    updateLVGLState(8, newState);

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

    applyBrightnessSetting(static_cast<int>(lv_slider_get_value(lv_event_get_target(e))), true);
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

    applyScreen2BacklightSetting(!lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED), true);
}

void oled_pixel_shift_toggle_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    applyPixelShiftSetting(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED), true);
}

void clock_button_toggle_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    applyClockButtonSetting(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED), true);
}

void oled_clock_saver_toggle_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    applyClockSaverSetting(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED), true);
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

    applyTimezoneSettingIndex(static_cast<int>(lv_roller_get_selected(lv_event_get_target(e))), false);
}

void timezone_done_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    saveSettingsToNVS();
    closeTimezoneModal();
}

void weather_setup_button_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    openWeatherSetupModal();
}

void weather_setup_done_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    closeWeatherSetupModal();
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
    syncScreen2IdleTimer();

    if (isMasterClockTileActive()) {
        updateMasterClock(true);
    }
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

void scheduleReboot(uint32_t delay_ms)
{
    reboot_scheduled = true;
    reboot_scheduled_at = millis() + delay_ms;
}

void processScheduledReboot()
{
    if (!reboot_scheduled) {
        return;
    }

    const long remaining = static_cast<long>(reboot_scheduled_at - millis());
    if (remaining <= 0) {
        ESP.restart();
    }
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
    tileMasterClock = lv_tileview_add_tile(tileview, 0, 2, LV_DIR_TOP | LV_DIR_BOTTOM);
    tileSett = lv_tileview_add_tile(tileview, 0, 3, LV_DIR_TOP);
    lv_obj_set_style_anim_time(tileMain, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_anim_time(tileSecond, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_anim_time(tileMaster, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_anim_time(tileMasterClock, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_anim_time(tileSett, 0, LV_PART_SCROLLBAR);
    hideScrollbarVisuals(tileview);
    hideScrollbarVisuals(tileMain);
    hideScrollbarVisuals(tileSecond);
    hideScrollbarVisuals(tileMaster);
    hideScrollbarVisuals(tileMasterClock);
    hideScrollbarVisuals(tileSett);
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
    lv_obj_set_size(ui_big_button, viewport_size, viewport_size);
    lv_obj_center(ui_big_button);
    lv_obj_set_style_radius(ui_big_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui_big_button, lv_color_black(), 0);
    lv_obj_set_style_bg_color(ui_big_button, active_switch_color, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui_big_button, active_switch_color, LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui_big_button, 0, 0);
    lv_obj_set_style_shadow_width(ui_big_button, 0, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui_big_button, 0, LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(ui_big_button, 0, LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_opa(ui_big_button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(ui_big_button, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_opa(ui_big_button, LV_OPA_TRANSP, LV_STATE_CHECKED);
    lv_obj_set_style_shadow_opa(ui_big_button, LV_OPA_TRANSP, LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_add_flag(ui_big_button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_clear_flag(ui_big_button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(ui_big_button, big_switch_event_handler, LV_EVENT_CLICKED, nullptr);

    ui_master_clock_button = lv_btn_create(tileMasterClock);
    lv_obj_set_size(ui_master_clock_button, viewport_size, viewport_size);
    lv_obj_center(ui_master_clock_button);
    lv_obj_set_style_radius(ui_master_clock_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ui_master_clock_button, 0, 0);
    lv_obj_set_style_shadow_width(ui_master_clock_button, 0, 0);
    lv_obj_set_style_pad_all(ui_master_clock_button, 0, 0);
    lv_obj_set_style_bg_opa(ui_master_clock_button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ui_master_clock_button, lv_color_black(), 0);
    lv_obj_add_flag(ui_master_clock_button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_clear_flag(ui_master_clock_button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(ui_master_clock_button, big_switch_event_handler, LV_EVENT_CLICKED, nullptr);

    master_clock_face = lv_obj_create(ui_master_clock_button);
    const lv_coord_t master_face_size = full_size - scaleUi(8);
    lv_obj_set_size(master_clock_face, master_face_size, master_face_size);
    lv_obj_center(master_clock_face);
    lv_obj_set_style_radius(master_clock_face, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(master_clock_face, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(master_clock_face, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(master_clock_face, 0, 0);
    lv_obj_set_style_pad_all(master_clock_face, 0, 0);
    hideScrollbarVisuals(master_clock_face);
    disableDecorationInteraction(master_clock_face);

    master_hour_hand = lv_line_create(master_clock_face);
    lv_obj_set_size(master_hour_hand, master_face_size, master_face_size);
    lv_obj_set_pos(master_hour_hand, 0, 0);
    lv_obj_set_style_line_width(master_hour_hand, scaleUi(6), 0);
    lv_obj_set_style_line_rounded(master_hour_hand, true, 0);
    disableDecorationInteraction(master_hour_hand);

    master_minute_hand = lv_line_create(master_clock_face);
    lv_obj_set_size(master_minute_hand, master_face_size, master_face_size);
    lv_obj_set_pos(master_minute_hand, 0, 0);
    lv_obj_set_style_line_width(master_minute_hand, scaleUi(4), 0);
    lv_obj_set_style_line_rounded(master_minute_hand, true, 0);
    disableDecorationInteraction(master_minute_hand);

    master_second_hand = lv_line_create(master_clock_face);
    lv_obj_set_size(master_second_hand, master_face_size, master_face_size);
    lv_obj_set_pos(master_second_hand, 0, 0);
    lv_obj_set_style_line_width(master_second_hand, scaleUi(2), 0);
    lv_obj_set_style_line_rounded(master_second_hand, true, 0);
    disableDecorationInteraction(master_second_hand);

    master_center_dot = lv_obj_create(master_clock_face);
    lv_obj_remove_style_all(master_center_dot);
    lv_obj_set_size(master_center_dot, scaleUi(10), scaleUi(10));
    lv_obj_center(master_center_dot);
    lv_obj_set_style_radius(master_center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(master_center_dot, LV_OPA_90, 0);
    disableDecorationInteraction(master_center_dot);

    master_weather_group = lv_obj_create(master_clock_face);
    lv_obj_remove_style_all(master_weather_group);
    lv_obj_set_size(master_weather_group, scaleUi(156), scaleUi(58));
    lv_obj_set_style_bg_opa(master_weather_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_layout(master_weather_group, LV_LAYOUT_FLEX, 0);
    lv_obj_set_flex_flow(master_weather_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(master_weather_group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(master_weather_group, 0, 0);
    lv_obj_set_style_pad_gap(master_weather_group, scaleUi(10), 0);
    lv_obj_add_flag(master_weather_group, LV_OBJ_FLAG_HIDDEN);
    hideScrollbarVisuals(master_weather_group);
    disableDecorationInteraction(master_weather_group);

    master_weather_icon = lv_obj_create(master_weather_group);
    lv_obj_remove_style_all(master_weather_icon);
    lv_obj_set_size(master_weather_icon, scaleUi(42), scaleUi(42));
    lv_obj_set_style_bg_opa(master_weather_icon, LV_OPA_TRANSP, 0);
    disableDecorationInteraction(master_weather_icon);

    master_weather_sun_core = lv_obj_create(master_weather_icon);
    lv_obj_remove_style_all(master_weather_sun_core);
    lv_obj_set_size(master_weather_sun_core, scaleUi(18), scaleUi(18));
    lv_obj_align(master_weather_sun_core, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(master_weather_sun_core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(master_weather_sun_core, LV_OPA_COVER, 0);
    disableDecorationInteraction(master_weather_sun_core);

    const lv_coord_t master_sun_center = scaleUi(21);
    const float master_sun_inner_radius = static_cast<float>(scaleUi(12));
    const float master_sun_outer_radius = static_cast<float>(scaleUi(18));
    for (int i = 0; i < 8; ++i) {
        const float angle_deg = -90.0f + static_cast<float>(i * 45);
        const float angle_rad = angle_deg * static_cast<float>(PI) / 180.0f;
        const float cos_value = cosf(angle_rad);
        const float sin_value = sinf(angle_rad);

        master_weather_sun_ray_points[i][0] = {
            static_cast<lv_coord_t>(lroundf(static_cast<float>(master_sun_center) + (cos_value * master_sun_inner_radius))),
            static_cast<lv_coord_t>(lroundf(static_cast<float>(master_sun_center) + (sin_value * master_sun_inner_radius)))};
        master_weather_sun_ray_points[i][1] = {
            static_cast<lv_coord_t>(lroundf(static_cast<float>(master_sun_center) + (cos_value * master_sun_outer_radius))),
            static_cast<lv_coord_t>(lroundf(static_cast<float>(master_sun_center) + (sin_value * master_sun_outer_radius)))};
        master_weather_sun_rays[i] = lv_line_create(master_weather_icon);
        lv_line_set_points(master_weather_sun_rays[i], master_weather_sun_ray_points[i], 2);
        lv_obj_set_style_line_width(master_weather_sun_rays[i], scaleUi(2), 0);
        lv_obj_set_style_line_rounded(master_weather_sun_rays[i], true, 0);
        lv_obj_set_style_line_opa(master_weather_sun_rays[i], LV_OPA_COVER, 0);
        hideScrollbarVisuals(master_weather_sun_rays[i]);
        disableDecorationInteraction(master_weather_sun_rays[i]);
    }

    const lv_coord_t master_cloud_rects[4][4] = {
        {scaleUi(3), scaleUi(14), scaleUi(15), scaleUi(15)},
        {scaleUi(13), scaleUi(8), scaleUi(18), scaleUi(18)},
        {scaleUi(26), scaleUi(14), scaleUi(13), scaleUi(13)},
        {scaleUi(8), scaleUi(21), scaleUi(28), scaleUi(11)},
    };
    for (int i = 0; i < 4; ++i) {
        master_weather_cloud_parts[i] = lv_obj_create(master_weather_icon);
        lv_obj_remove_style_all(master_weather_cloud_parts[i]);
        lv_obj_set_size(master_weather_cloud_parts[i], master_cloud_rects[i][2], master_cloud_rects[i][3]);
        lv_obj_set_pos(master_weather_cloud_parts[i], master_cloud_rects[i][0], master_cloud_rects[i][1]);
        lv_obj_set_style_radius(master_weather_cloud_parts[i], (i == 3) ? scaleUi(6) : LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(master_weather_cloud_parts[i], LV_OPA_COVER, 0);
        disableDecorationInteraction(master_weather_cloud_parts[i]);
    }

    const lv_coord_t master_rain_rects[3][4] = {
        {scaleUi(11), scaleUi(31), scaleUi(2), scaleUi(8)},
        {scaleUi(20), scaleUi(33), scaleUi(2), scaleUi(8)},
        {scaleUi(29), scaleUi(31), scaleUi(2), scaleUi(8)},
    };
    for (int i = 0; i < 3; ++i) {
        master_weather_rain_lines[i] = lv_obj_create(master_weather_icon);
        lv_obj_remove_style_all(master_weather_rain_lines[i]);
        lv_obj_set_size(master_weather_rain_lines[i], master_rain_rects[i][2], master_rain_rects[i][3]);
        lv_obj_set_pos(master_weather_rain_lines[i], master_rain_rects[i][0], master_rain_rects[i][1]);
        lv_obj_set_style_radius(master_weather_rain_lines[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(master_weather_rain_lines[i], LV_OPA_COVER, 0);
        disableDecorationInteraction(master_weather_rain_lines[i]);
    }

    master_weather_temp_label = lv_label_create(master_weather_group);
    lv_label_set_text(master_weather_temp_label, "--°");
    lv_obj_set_style_text_font(master_weather_temp_label, &lv_font_montserrat_28, 0);
    disableDecorationInteraction(master_weather_temp_label);

    ui_master_clock_info = lv_label_create(tileMasterClock);
    lv_label_set_text(
        ui_master_clock_info,
        "Enable Clock Button on the\n"
        "Web UI to use the\n"
        "B1 clock screen.");
    lv_obj_set_width(ui_master_clock_info, full_size - scaleUi(60));
    lv_obj_align(ui_master_clock_info, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_align(ui_master_clock_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_master_clock_info, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ui_master_clock_info, lv_color_white(), 0);
    disableDecorationInteraction(ui_master_clock_info);

    lv_obj_t *lbl_set = lv_label_create(tileSett);
    lv_label_set_text(lbl_set, "Settings");
    lv_obj_align(lbl_set, LV_ALIGN_TOP_MID, 0, scaleUi(14));
    lv_obj_set_style_text_font(lbl_set, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_set, lv_color_white(), 0);

    ui_settings_web_links_label = lv_label_create(tileSett);
    lv_obj_set_width(ui_settings_web_links_label, full_size - scaleUi(88));
    lv_label_set_long_mode(ui_settings_web_links_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui_settings_web_links_label, LV_ALIGN_TOP_MID, 0, scaleUi(78));
    lv_obj_set_style_text_align(ui_settings_web_links_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_settings_web_links_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_line_space(ui_settings_web_links_label, scaleUi(4), 0);
    lv_obj_set_style_text_color(ui_settings_web_links_label, lv_color_make(210, 210, 210), 0);
    updateSettingsWebLinksLabel();

    lv_obj_t *lbl_bri_title = lv_label_create(tileSett);
    lv_label_set_text(lbl_bri_title, "Display Brightness");
    lv_obj_set_style_text_color(lbl_bri_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_bri_title, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_bri_title, LV_ALIGN_TOP_MID, 0, scaleUi(230));

    ui_brightness_slider = lv_slider_create(tileSett);
    lv_obj_set_width(ui_brightness_slider, full_size - scaleUi(88));
    lv_obj_set_height(ui_brightness_slider, scaleUi(22));
    lv_obj_align(ui_brightness_slider, LV_ALIGN_TOP_MID, 0, scaleUi(260));
    lv_obj_set_style_radius(ui_brightness_slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_brightness_slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui_brightness_slider, lv_color_make(48, 48, 48), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_brightness_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_brightness_slider, active_switch_color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui_brightness_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui_brightness_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(ui_brightness_slider, scaleUi(7), LV_PART_KNOB);
    lv_slider_set_range(ui_brightness_slider, 1, boardProfile().brightness_levels);
    lv_slider_set_value(ui_brightness_slider, global_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *bl_cont = lv_obj_create(tileSett);
    lv_obj_set_size(bl_cont, full_size - scaleUi(120), scaleUi(66));
    lv_obj_align(bl_cont, LV_ALIGN_TOP_MID, 0, scaleUi(304));
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
        "Allow Screen 2 to turn off\n"
        "after inactivity");
    lv_obj_set_width(bl_lbl, full_size - scaleUi(210));
    lv_obj_set_style_text_color(bl_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(bl_lbl, &lv_font_montserrat_16, 0);

    ui_bl_toggle = lv_switch_create(bl_cont);
    if (!screen2_bl_always_on) {
        lv_obj_add_state(ui_bl_toggle, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(ui_bl_toggle, bl_toggle_event_handler, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *lbl_home_status = lv_label_create(tileSett);
    lv_label_set_text(lbl_home_status, "HOMEsmthng");
    lv_obj_set_style_text_font(lbl_home_status, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_home_status, lv_color_white(), 0);
    lv_obj_align(lbl_home_status, LV_ALIGN_TOP_MID, 0, scaleUi(386));

    ui_wifi_label = lv_label_create(tileSett);
    lv_label_set_text(ui_wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(ui_wifi_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ui_wifi_label, lv_color_white(), 0);
    lv_obj_align(ui_wifi_label, LV_ALIGN_BOTTOM_MID, 0, -scaleUi(14));

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

    const lv_coord_t sun_center = scaleUi(21);
    const float sun_inner_radius = static_cast<float>(scaleUi(12));
    const float sun_outer_radius = static_cast<float>(scaleUi(18));
    for (int i = 0; i < 8; ++i) {
        const float angle_deg = -90.0f + static_cast<float>(i * 45);
        const float angle_rad = angle_deg * static_cast<float>(PI) / 180.0f;
        const float cos_value = cosf(angle_rad);
        const float sin_value = sinf(angle_rad);

        screensaver_weather_sun_ray_points[i][0] = {
            static_cast<lv_coord_t>(lroundf(static_cast<float>(sun_center) + (cos_value * sun_inner_radius))),
            static_cast<lv_coord_t>(lroundf(static_cast<float>(sun_center) + (sin_value * sun_inner_radius)))};
        screensaver_weather_sun_ray_points[i][1] = {
            static_cast<lv_coord_t>(lroundf(static_cast<float>(sun_center) + (cos_value * sun_outer_radius))),
            static_cast<lv_coord_t>(lroundf(static_cast<float>(sun_center) + (sin_value * sun_outer_radius)))};
        screensaver_weather_sun_rays[i] = lv_line_create(screensaver_weather_icon);
        lv_line_set_points(screensaver_weather_sun_rays[i], screensaver_weather_sun_ray_points[i], 2);
        lv_obj_set_style_line_width(screensaver_weather_sun_rays[i], scaleUi(2), 0);
        lv_obj_set_style_line_rounded(screensaver_weather_sun_rays[i], true, 0);
        lv_obj_set_style_line_opa(screensaver_weather_sun_rays[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(screensaver_weather_sun_rays[i], LV_OBJ_FLAG_SCROLLABLE);
        hideScrollbarVisuals(screensaver_weather_sun_rays[i]);
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
    updateMasterClockAppearance();
    setScreensaverWeatherGlyph(weather_glyph);
    setMasterClockWeatherGlyph(weather_glyph);
    updateWeatherMonogram();
    updateScreensaverClock(true);
    updateMasterClockMode();
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
    loadOrCreateDeviceConfig();

    HOMEKIT_PAIRING_CODE = HOMEKIT_PAIRING_CODE_STR.c_str();
    homeSpan.setPairingCode(HOMEKIT_PAIRING_CODE);
    homeSpan.setQRID(kHomeKitSetupId);

    if (!displayBackend().begin()) {
        Serial.println("Display initialization failed");
        while (true) {
            delay(1000);
        }
    }

    setupUI();
    applyTimezoneSetting(true);

    StoredWifiCredentials wifi_credentials = {};
    if (readStoredWifiCredentials(wifi_credentials)) {
        const String final_bridge_name = String("HOMEsmthng ") + BRIDGE_SUFFIX_STR;
        homeSpan.setStatusCallback(homeSpanStatusCallback);
        homeSpan.begin(Category::Bridges, final_bridge_name.c_str(), device_host_name.c_str());
        homeSpan.setHostNameSuffix("");
        homespan_started = true;

        startWeatherConfigServer();

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
    } else {
        startProvisioningMode();
    }
    updateMasterClockMode();

    delay(100);
    updateWiFiSymbol();
    updateTimezoneStatusLabel();
    applyPixelShiftOffset(0, 0);
    safeSetBrightness(global_brightness);
    logWeatherConfigUrls();
}

void loop()
{
    lv_timer_handler();
    if (homespan_started) {
        homeSpan.poll();
    }
    ensureWeatherConfigServer();
    if (weather_config_server_running) {
        weather_config_server.handleClient();
    }
    if (provisioning_server_running) {
        provisioning_dns_server.processNextRequest();
        provisioning_server.handleClient();
    }

    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1000) {
        lastUpdate = millis();
        updateWiFiSymbol();
        if (homespan_started) {
            ensureTimeIsConfigured();
            updateWeatherIfNeeded();
            if (isMasterClockTileActive()) {
                updateMasterClock(false);
            }
        } else {
            updateWeatherSetupButtonLabel();
            updateWeatherSetupModalContent();
        }
        logWeatherConfigUrls();
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
    processScheduledReboot();
    delay(5);
}
