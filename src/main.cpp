/**
 * @file      main.cpp
 * @author    modsmthng
 * @date      2026-03-23
 *
 * High-level firmware orchestration. Feature-specific state, persistence and
 * reusable helpers live in smaller modules; this file still owns the LVGL and
 * HomeSpan callback graph while the remaining UI/web code is split safely.
 */

#include <Arduino.h>
#include <HomeSpan.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <lvgl.h>

#include <math.h>
#include <time.h>

#include "app_state.h"
#include "app_utils.h"
#include "battery_monitor.h"
#include "display_backend.h"
#include "settings_store.h"
#include "weather.h"
#include "clock_ui.h"
#include "web_admin.h"
#include "ui_setup.h"

// Internal (file-scope) forward declarations
static void updateWifiSetupMessage();
static void homeSpanStatusCallback(HS_STATUS status);
static void ensureTimeIsConfigured();
static void updateTimezoneButtonLabel();
static void updateTimezoneStatusLabel();
static void updateWeatherSetupButtonLabel();
static void updateWeatherSetupModalContent();

MySwitch *homekit_switches[kNumSwitches] = {};

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
bool isMasterClockTileActive()
{
    return tileview && tileMasterClock && lv_tileview_get_tile_act(tileview) == tileMasterClock;
}

void updateTimezoneButtonLabel()
{
    if (!ui_timezone_value_label) {
        return;
    }

    lv_label_set_text_fmt(ui_timezone_value_label, "%s  " LV_SYMBOL_RIGHT, selectedTimezoneLabel());
}



void updateWeatherSetupButtonLabel()
{
    if (!ui_weather_setup_value_label) {
        return;
    }

    lv_label_set_text(ui_weather_setup_value_label, "Web UI " LV_SYMBOL_RIGHT);
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

void applyBrightnessSetting(int brightness, bool persist)
{
    global_brightness = constrain(brightness, 1, boardProfile().brightness_levels);
    if (ui_brightness_slider) {
        lv_slider_set_value(ui_brightness_slider, global_brightness, LV_ANIM_OFF);
    }
    safeSetBrightness(currentBrightnessTarget());

    if (persist) {
        saveSettingsToNVS();
    }
}

void applyDisplayRotationSetting(int degrees, bool persist)
{
    display_ui_rotation_degrees = normalizeDisplayRotationDegrees(degrees);
    displayBackend().setUiRotation(static_cast<uint16_t>(display_ui_rotation_degrees));

    if (persist) {
        saveSettingsToNVS();
    }
}

void applyScreen2BacklightSetting(bool always_on, bool persist)
{
    screen2_bl_always_on = always_on;
    if (ui_bl_toggle) {
        if (screen2_bl_always_on) {
            lv_obj_clear_state(ui_bl_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(ui_bl_toggle, LV_STATE_CHECKED);
        }
    }

    syncScreen2IdleTimer();
    safeSetBrightness(currentBrightnessTarget());

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

    displayBackend().setUiRotation(static_cast<uint16_t>(display_ui_rotation_degrees));
    if (!displayBackend().begin()) {
        Serial.println("Display initialization failed");
        while (true) {
            delay(1000);
        }
    }

    batteryMonitor().begin();
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
    safeSetBrightness(currentBrightnessTarget());
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

    if (batteryMonitor().update()) {
        updateBatteryStatusLabel();
    }

    updateOledProtection();

    safeSetBrightness(currentBrightnessTarget());
    processScheduledReboot();
    delay(5);
}
