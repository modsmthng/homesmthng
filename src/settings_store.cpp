// NVS-backed settings and device identity.
//
// The keys remain unchanged so existing devices keep their settings after the
// refactor. Defaults are only used when a key does not exist yet.

#include "settings_store.h"

#include <nvs.h>

#include "app_utils.h"

namespace {

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

} // namespace

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

void saveSettingsToNVS()
{
    preferences.begin("homespan", false);
    preferences.putInt("brightness", global_brightness);
    preferences.putUInt("color_hex", active_switch_color_hex);
    preferences.putInt("ui_rot", display_ui_rotation_degrees);
    preferences.putBool("screen2_bl", screen2_bl_always_on);
    preferences.putBool("px_shift", oled_pixel_shift_enabled);
    preferences.putBool("clock_btn", clock_button_screen_enabled);
    preferences.putBool("clock_first", clock_button_screen_first);
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
    display_ui_rotation_degrees = normalizeDisplayRotationDegrees(preferences.getInt("ui_rot", kDefaultDisplayRotationDegrees));
    screen2_bl_always_on = preferences.getBool("screen2_bl", true);
    oled_pixel_shift_enabled = preferences.getBool("px_shift", false);
    oled_clock_saver_enabled = preferences.getBool("clock_sv", supportsClockSaver());
    clock_button_screen_enabled = preferences.getBool("clock_btn", supportsClockSaver());
    clock_button_screen_first = preferences.getBool("clock_first", false);
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
