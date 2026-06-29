#include "web_admin.h"
#include "web_pages.h"

#include <qrcodegen.h>

#include "app_utils.h"
#include "battery_monitor.h"
#include "settings_store.h"
#include "weather.h"
#include "clock_ui.h"

namespace {

bool parseLongFormValue(const String &text, long &value)
{
    String trimmed = text;
    trimmed.trim();
    if (trimmed.isEmpty()) {
        return false;
    }

    char *end = nullptr;
    value = strtol(trimmed.c_str(), &end, 10);
    return end != trimmed.c_str() && end && *end == '\0';
}

} // namespace

// ---------------------------------------------------------------------------
// URL / misc helpers
// ---------------------------------------------------------------------------

String weatherConfigStationUrl()
{
    return adminMdnsUrl();
}

String weatherConfigApUrl()
{
    return setupPortalUrl();
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

String setupPortalUrl()
{
    if (!provisioning_mode_active) {
        return String();
    }
    return String("http://192.168.4.1");
}

String setupAccessPointSsid()
{
    return String("homesmthng");
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

String formattedPairingCode()
{
    if (HOMEKIT_PAIRING_CODE_STR.length() != 8) {
        return HOMEKIT_PAIRING_CODE_STR;
    }
    return HOMEKIT_PAIRING_CODE_STR.substring(0, 4) + "-" + HOMEKIT_PAIRING_CODE_STR.substring(4, 8);
}

String homeKitSetupPayload()
{
    HapQR qr;
    return String(qr.get(static_cast<uint32_t>(HOMEKIT_PAIRING_CODE_STR.toInt()), kHomeKitSetupId, static_cast<uint8_t>(Category::Bridges)));
}

bool isSetupAccessPointVisible()
{
    return provisioning_mode_active;
}

// ---------------------------------------------------------------------------
// QR code SVG rendering
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Page rendering
// ---------------------------------------------------------------------------

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
    const int standby_brightness_scale_value = brightnessScaleForWeb(standby_brightness);
    String normalized_active_section = active_section;
    normalized_active_section.toLowerCase();
    const bool open_homekit = normalized_active_section == "homekit";
    const bool open_device = normalized_active_section == "device";
    const bool open_wifi = normalized_active_section == "wifi";
    const bool open_display = normalized_active_section == "display";
    const bool open_standby = normalized_active_section == "standby";
    const bool open_time = normalized_active_section == "time";
    const bool open_weather = normalized_active_section == "weather";
    const bool open_battery = normalized_active_section == "battery";

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

    // HEAD
    page += FPSTR(ADMIN_HEAD_OPEN);
    page += FPSTR(SHARED_CSS);
    page += FPSTR(ADMIN_HEAD_CLOSE);

    // Message card (conditional)
    if (message.length() > 0) {
        page += F("<div class=\"msg");
        if (is_error) {
            page += F(" error");
        }
        page += "\">" + escaped_message + F("</div>");
    }

    // Status card
    page += FPSTR(ADMIN_STATUS_CARD_START);
    page += escaped_device_label;
    page += FPSTR(ADMIN_STATUS_CARD_INTRO);
    page += escaped_device_host;
    page += FPSTR(ADMIN_STATUS_CARD_WIFI);
    page += htmlEscape(wifi_status);
    if (mdns_url.length() > 0) {
        page += FPSTR(ADMIN_STATUS_CARD_MDNS);
        page += escaped_mdns_url;
    }
    if (direct_url.length() > 0) {
        page += FPSTR(ADMIN_STATUS_CARD_DIRECT);
        page += escaped_direct_url;
    }
    if (setup_url.length() > 0) {
        page += FPSTR(ADMIN_STATUS_CARD_SETUP);
        page += escaped_setup_url;
    }
    page += FPSTR(ADMIN_STATUS_CARD_HOMEKIT);
    page += homekit_is_paired ? F("Paired") : F("Not paired yet");
    page += FPSTR(ADMIN_STATUS_CARD_END);

    // HomeKit section
    page += FPSTR(ADMIN_SECTION_OPEN);
    if (open_homekit) {
        page += F(" open");
    }
    page += FPSTR(ADMIN_SECTION_HOMEKIT_HEAD);
    if (homekit_is_paired) {
        page += FPSTR(ADMIN_SECTION_HOMEKIT_PAIRED);
    } else {
        page += FPSTR(ADMIN_SECTION_HOMEKIT_UNPAIRED_HEAD);
        page += htmlEscape(pairing_code);
        page += FPSTR(ADMIN_SECTION_HOMEKIT_UNPAIRED_MID);
        page += htmlEscape(pairing_payload);
        page += FPSTR(ADMIN_SECTION_HOMEKIT_UNPAIRED_END);
    }
    page += FPSTR(ADMIN_SECTION_CLOSE);

    // Device section
    page += FPSTR(ADMIN_SECTION_OPEN);
    if (open_device) {
        page += F(" open");
    }
    page += FPSTR(ADMIN_SECTION_DEVICE);
    page += escaped_device_label;
    page += FPSTR(ADMIN_SECTION_DEVICE_MID);
    page += escaped_device_host;
    page += FPSTR(ADMIN_SECTION_DEVICE_END);

    // Wi-Fi section
    page += FPSTR(ADMIN_SECTION_OPEN);
    if (open_wifi) {
        page += F(" open");
    }
    page += FPSTR(ADMIN_SECTION_WIFI);
    page += escaped_stored_ssid;
    page += FPSTR(ADMIN_SECTION_WIFI_END);

    // Display section
    page += FPSTR(ADMIN_SECTION_OPEN);
    if (open_display) {
        page += F(" open");
    }
    page += FPSTR(ADMIN_SECTION_DISPLAY_HEAD);
    page += String(kWebBrightnessScaleMax);
    page += FPSTR(ADMIN_SECTION_DISPLAY_BRIGHTNESS_MID);
    page += String(brightness_scale_value);
    page += FPSTR(ADMIN_SECTION_DISPLAY_BRIGHTNESS_END);
    page += active_color_css;
    page += FPSTR(ADMIN_SECTION_DISPLAY_COLOR_MID);
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
    page += F("<p class=\"subtle\">Enter a value like #68724D.</p></div>");
    page += F("<div class=\"checkboxes\">");
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
    page += F("<p class=\"subtle\">Shows a separate full-screen clock button page.</p></div>");
    page += F("<div class=\"toggle-item\"><label for=\"screen_order\">Screen order</label>"
              "<select id=\"screen_order\" name=\"screen_order\">");
    page += "<option value=\"big_first\"";
    if (!clock_button_screen_first) {
        page += " selected";
    }
    page += ">Large B1 button first</option>";
    page += "<option value=\"clock_first\"";
    if (clock_button_screen_first) {
        page += " selected";
    }
    page += ">Clock button first</option>";
    page += F("</select><p class=\"subtle\">Choose which B1 page appears first below Screen 1. Restart to apply the new order.</p></div>");
    page += "<div class=\"toggle-item\"><label class=\"checkbox-label\"><input type=\"checkbox\" name=\"screen2_auto_off\"";
    if (!screen2_bl_always_on) {
        page += " checked";
    }
    page += ">Large B1 auto-off</label>";
    page += F("<p class=\"subtle\">Turns off the display backlight after five seconds on the large B1 screen. Tap or leave the screen to turn it back on.</p></div>");
    page += F("</div><details class=\"manual-location\"><summary>Experimental</summary><div class=\"manual-body\">"
              "<label for=\"display_rotation\">Display UI orientation</label>"
              "<select id=\"display_rotation\" name=\"display_rotation\">");
    const int rotation_options[] = {0, 90, 180, 270};
    for (int i = 0; i < 4; ++i) {
        const int rotation = rotation_options[i];
        page += "<option value=\"" + String(rotation) + "\"";
        if (display_ui_rotation_degrees == rotation) {
            page += " selected";
        }
        page += ">" + String(rotation) + "&deg;</option>";
    }
    page += F("</select>"
              "<p class=\"subtle\">Rotates the on-device display UI and touch input in 90 degree steps. Non-zero rotations can show panel artifacts on some devices.</p>"
              "</div></details><div class=\"actions\">"
              "<button class=\"primary\" type=\"submit\">Save display settings</button>"
              "<button class=\"secondary\" type=\"submit\" name=\"restart_after_save\" value=\"1\">Save &amp; restart</button>"
              "</div></form></div></details>");

    // Standby section
    page += "<details class=\"card section-card\" id=\"standby-section\"";
    if (open_standby) {
        page += " open";
    }
    page += F("><summary>Standby</summary><div class=\"section-body\">"
              "<p>Choose what the display does after a period without touch input. HomeSpan and the interface keep running in every mode.</p>"
              "<form method=\"POST\" action=\"/settings/standby#standby-section\">"
              "<input type=\"hidden\" name=\"section\" value=\"standby\">"
              "<label for=\"standby_mode\">Standby mode</label>"
              "<select id=\"standby_mode\" name=\"standby_mode\">");
    const struct {
        StandbyMode mode;
        const char *value;
        const char *label;
    } standby_options[] = {
        {StandbyMode::Disabled, "disabled", "Standby disabled"},
        {StandbyMode::DisplayOff, "display_off", "Display off"},
        {StandbyMode::Dim, "dim", "Dim display"},
        {StandbyMode::ClockSaver, "clock", "Clock Saver"},
    };
    for (const auto &option : standby_options) {
        page += "<option value=\"" + String(option.value) + "\"";
        if (standby_mode == option.mode) {
            page += " selected";
        }
        page += ">" + String(option.label) + "</option>";
    }
    page += F("</select>"
              "<p class=\"subtle\">The first touch wakes the display without activating the control underneath. On the large B1 and Clock Button screens, Clock Saver dims the existing screen instead of showing another clock.</p>"
              "<label for=\"standby_idle_s\">Standby time</label>");
    page += "<input id=\"standby_idle_s\" name=\"standby_idle_s\" type=\"number\" min=\"10\" max=\"3600\" step=\"5\" value=\""
        + String(standby_idle_ms / 1000UL) + "\">";
    page += F("<p class=\"subtle\">Seconds of inactivity before standby starts.</p>"
              "<div id=\"standby_brightness_wrap\" class=\"setting-block\">"
              "<label for=\"standby_brightness_scale\">Standby brightness</label>"
              "<div class=\"stepper\">"
              "<button class=\"secondary step-btn\" type=\"button\" onclick=\"adjustStandbyBrightness(-1)\" aria-label=\"Dunkler\">&minus;</button>");
    page += "<input id=\"standby_brightness_scale\" name=\"standby_brightness_scale\" type=\"number\" min=\"0\" max=\""
        + String(kWebBrightnessScaleMax) + "\" step=\"1\" value=\"" + String(standby_brightness_scale_value) + "\">";
    page += F("<button class=\"secondary step-btn\" type=\"button\" onclick=\"adjustStandbyBrightness(1)\" aria-label=\"Heller\">+</button>"
              "</div>"
              "<p class=\"subtle\">Used by Dim display and Clock Saver. If the main brightness is lower, the main brightness wins temporarily.</p>"
              "</div>"
              "<div class=\"actions\"><button class=\"primary\" type=\"submit\">Save standby settings</button></div>"
              "</form></div></details>");

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
    page += FPSTR(ADMIN_SECTION_TIME_END);

    // Weather section
    page += FPSTR(ADMIN_SECTION_OPEN_WEATHER);
    if (open_weather) {
        page += F(" open");
    }
    page += FPSTR(ADMIN_SECTION_WEATHER_HEAD);
    if (weather_location_set) {
        // Visibility form
        page += FPSTR(ADMIN_SECTION_WEATHER_VISIBILITY_FORM);
        page += escaped_label;
        page += FPSTR(ADMIN_SECTION_WEATHER_VISIBILITY_LAT);
        page += String(weather_custom_latitude, 6);
        page += FPSTR(ADMIN_SECTION_WEATHER_VISIBILITY_LON);
        page += String(weather_custom_longitude, 6);
        page += weather_enabled
            ? FPSTR(ADMIN_SECTION_WEATHER_VISIBILITY_TOGGLE_CHECKED)
            : FPSTR(ADMIN_SECTION_WEATHER_VISIBILITY_TOGGLE_UNCHECKED);
        page += FPSTR(ADMIN_SECTION_WEATHER_VISIBILITY_END);
    }

    // Weather info
    page += FPSTR(ADMIN_SECTION_WEATHER_INFO);
    page += weather_enabled
        ? FPSTR(ADMIN_SECTION_WEATHER_INFO_ENABLED)
        : FPSTR(ADMIN_SECTION_WEATHER_INFO_DISABLED);
    page += FPSTR(ADMIN_SECTION_WEATHER_INFO_SOURCE);
    page += String(weather_location_set ? escaped_label : "No location set");
    if (weather_location_set) {
        page += FPSTR(ADMIN_SECTION_WEATHER_INFO_COORDS);
        page += String(weather_custom_latitude, 6) + ", " + String(weather_custom_longitude, 6);
    }
    page += FPSTR(ADMIN_SECTION_WEATHER_INFO_CURRENT);
    page += weather_status;
    page += FPSTR(ADMIN_SECTION_WEATHER_INFO_CLOSE);

    // Search form
    page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_FORM);
    page += escaped_weather_search_value;
    page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_PINBTN);

    // Browser location hidden form
    page += FPSTR(ADMIN_SECTION_WEATHER_BROWSER_FORM);

    // Search results message
    if (search_message.length() > 0) {
        page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_MSG);
        if (search_is_error) {
            page += F(" error");
        }
        page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_MSG_END);
        page += escaped_search_message;
        page += F("</div>");
    }

    // Search result items
    if (search_results && search_result_count > 0) {
        page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_RESULTS_OPEN);
        for (int i = 0; i < search_result_count; ++i) {
            if (!search_results[i].valid) continue;

            const String display_name = weatherSearchDisplayName(search_results[i]);
            const String escaped_display_name = htmlEscape(display_name);
            const String escaped_result_name = htmlEscape(display_name);

            page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_RESULT_FORM);
            page += escaped_result_name;
            page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_RESULT_LAT);
            page += String(search_results[i].latitude, 6);
            page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_RESULT_LON);
            page += String(search_results[i].longitude, 6);
            page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_RESULT_BTN);
            page += escaped_display_name;
            page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_RESULT_META);
            page += String(search_results[i].latitude, 6) + ", " + String(search_results[i].longitude, 6);
            page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_RESULT_CLOSE);
        }
        page += FPSTR(ADMIN_SECTION_WEATHER_SEARCH_RESULTS_CLOSE);
    }

    // Manual coordinates
    page += FPSTR(ADMIN_SECTION_WEATHER_MANUAL);
    page += escaped_name;
    page += FPSTR(ADMIN_SECTION_WEATHER_MANUAL_MID);
    page += String(weather_location_set ? String(weather_custom_latitude, 6) : String());
    page += FPSTR(ADMIN_SECTION_WEATHER_MANUAL_LAT_END);
    page += String(weather_location_set ? String(weather_custom_longitude, 6) : String());
    page += FPSTR(ADMIN_SECTION_WEATHER_MANUAL_END);

    // Clear weather
    page += FPSTR(ADMIN_SECTION_WEATHER_CLEAR);
    page += FPSTR(ADMIN_SECTION_CLOSE);

    // Battery section
    page += FPSTR(ADMIN_SECTION_OPEN);
    if (open_battery) {
        page += F(" open");
    }
    page += FPSTR(ADMIN_SECTION_BATTERY);
    {
        const BatteryStatus &bat = batteryMonitor().status();
        const bool has_backend = boardProfile().battery_backend != BatteryBackendKind::None;

        if (!has_backend) {
            page += FPSTR(ADMIN_SECTION_BATTERY_NO_BACKEND);
        } else if (!bat.telemetry_available && bat.warning == BatteryWarningKind::None) {
            page += FPSTR(ADMIN_SECTION_BATTERY_NO_TELEMETRY);
        } else {
            page += FPSTR(ADMIN_SECTION_BATTERY_STATUS_OPEN);
            page += htmlEscape(batteryStatusPrimaryText(bat));
            if (bat.charge_state != BatteryChargeState::Unknown || bat.warning != BatteryWarningKind::None || bat.voltage_mv > 0) {
                page += FPSTR(ADMIN_SECTION_BATTERY_CONDITION);
                page += htmlEscape(batteryStatusSecondaryText(bat));
            }
            if (bat.voltage_mv > 0) {
                page += FPSTR(ADMIN_SECTION_BATTERY_VOLTAGE);
                page += String(static_cast<float>(bat.voltage_mv) / 1000.0f, 3) + " V";
            }
            if (bat.warning != BatteryWarningKind::None) {
                String warning_text;
                switch (bat.warning) {
                case BatteryWarningKind::ReadFailed:
                    warning_text = "Read failed";
                    break;
                case BatteryWarningKind::ChargerFault:
                    warning_text = "Charger fault";
                    break;
                case BatteryWarningKind::ThermalLimit:
                    warning_text = "Thermal limit";
                    break;
                default:
                    warning_text = "Unknown";
                    break;
                }
                page += FPSTR(ADMIN_SECTION_BATTERY_WARNING);
                page += htmlEscape(warning_text);
            }
            page += FPSTR(ADMIN_SECTION_BATTERY_STATUS_CLOSE);
            if (bat.approximate) {
                page += FPSTR(ADMIN_SECTION_BATTERY_APPROX);
            }
        }
    }
    page += FPSTR(ADMIN_SECTION_BATTERY_END);

    // JS + footer
    page += FPSTR(ADMIN_JS);
    page += FPSTR(ADMIN_FOOTER);

    return page;
}

String renderProvisioningPage(const String &message, bool is_error)
{
    const String escaped_message = htmlEscape(message);
    const String escaped_label = htmlEscape(device_label);
    const String escaped_host = htmlEscape(device_host_name);
    const String escaped_ssid = htmlEscape(setupAccessPointSsid());
    String page;
    page.reserve(9000);

    page += FPSTR(PROVISIONING_HEAD_OPEN);
    page += FPSTR(SHARED_CSS);
    page += FPSTR(PROVISIONING_CSS);
    page += FPSTR(PROVISIONING_HEAD_CLOSE);
    page += escaped_ssid;
    page += FPSTR(PROVISIONING_AP_END);

    if (message.length() > 0) {
        page += F("<div class=\"msg");
        if (is_error) {
            page += F(" error");
        }
        page += "\">" + escaped_message + F("</div>");
    }

    page += FPSTR(PROVISIONING_FORM_HEAD);
    page += escaped_label;
    page += FPSTR(PROVISIONING_FORM_MID);
    page += escaped_host;
    page += FPSTR(PROVISIONING_FORM_TAIL);
    page += FPSTR(PROVISIONING_SCRIPT);

    return page;
}

// ---------------------------------------------------------------------------
// Web handlers — admin server
// ---------------------------------------------------------------------------

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

    if (visibility_only) {
        if (!hasCustomWeatherLocation()) {
            weather_config_server.send(400, "text/html; charset=utf-8", renderAdminPage("Set a weather location before enabling weather.", true, "", nullptr, 0, "", false, "weather"));
            return;
        }

        applyWeatherEnabledSetting(next_weather_enabled, true);
        redirectToWeatherSection();
        return;
    }

    float latitude = 0.0f;
    float longitude = 0.0f;
    const String latitude_arg = weather_config_server.arg("latitude");
    const String longitude_arg = weather_config_server.arg("longitude");

    if (!parseFloatParameter(latitude_arg, latitude) || !parseFloatParameter(longitude_arg, longitude) ||
        !isValidWeatherCoordinates(latitude, longitude)) {
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

    int next_brightness = global_brightness;
    if (weather_config_server.hasArg("brightness_scale")) {
        String brightness_arg = weather_config_server.arg("brightness_scale");
        brightness_arg.trim();
        if (!brightness_arg.isEmpty()) {
            long brightness_scale = 0;
            if (!parseLongFormValue(brightness_arg, brightness_scale)
                || brightness_scale < 0 || brightness_scale > kWebBrightnessScaleMax) {
                weather_config_server.send(
                    400,
                    "text/html; charset=utf-8",
                    renderAdminPage("Display brightness must be between 0 and 10.", true, "", nullptr, 0, "", false, "display"));
                return;
            }
            next_brightness = brightnessLevelFromWebScale(static_cast<int>(brightness_scale));
        }
    }

    active_switch_color_hex = next_color_hex;
    applyNewColor(lv_color_hex(active_switch_color_hex));
    applyBrightnessSetting(next_brightness, false);
    const int display_rotation = weather_config_server.hasArg("display_rotation")
                                     ? weather_config_server.arg("display_rotation").toInt()
                                     : display_ui_rotation_degrees;
    applyDisplayRotationSetting(display_rotation, false);
    applyScreen2BacklightSetting(!weather_config_server.hasArg("screen2_auto_off"), false);
    applyPixelShiftSetting(weather_config_server.hasArg("px_shift"), false);
    applyClockButtonSetting(weather_config_server.hasArg("clock_btn"), false);
    clock_button_screen_first = weather_config_server.arg("screen_order") == "clock_first";
    saveSettingsToNVS();

    if (weather_config_server.hasArg("restart_after_save")) {
        scheduleReboot();
        weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("Display settings saved. Restarting now.", false, "", nullptr, 0, "", false, "display"));
        return;
    }

    weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("Display settings saved.", false, "", nullptr, 0, "", false, "display"));
}

void handleStandbySettingsSave()
{
    const String requested_mode = weather_config_server.arg("standby_mode");
    StandbyMode next_mode;
    if (requested_mode == "disabled") {
        next_mode = StandbyMode::Disabled;
    } else if (requested_mode == "display_off") {
        next_mode = StandbyMode::DisplayOff;
    } else if (requested_mode == "dim") {
        next_mode = StandbyMode::Dim;
    } else if (requested_mode == "clock") {
        next_mode = StandbyMode::ClockSaver;
    } else {
        weather_config_server.send(400, "text/html; charset=utf-8", renderAdminPage("Please choose a valid standby mode.", true, "", nullptr, 0, "", false, "standby"));
        return;
    }

    long idle_seconds = 0;
    if (!parseLongFormValue(weather_config_server.arg("standby_idle_s"), idle_seconds)
        || idle_seconds < static_cast<long>(kStandbyMinIdleMs / 1000UL)
        || idle_seconds > static_cast<long>(kStandbyMaxIdleMs / 1000UL)) {
        weather_config_server.send(400, "text/html; charset=utf-8", renderAdminPage("Standby time must be between 10 and 3600 seconds.", true, "", nullptr, 0, "", false, "standby"));
        return;
    }

    long brightness_scale = 0;
    if (!parseLongFormValue(weather_config_server.arg("standby_brightness_scale"), brightness_scale)
        || brightness_scale < 0 || brightness_scale > kWebBrightnessScaleMax) {
        weather_config_server.send(400, "text/html; charset=utf-8", renderAdminPage("Standby brightness must be between 0 and 10.", true, "", nullptr, 0, "", false, "standby"));
        return;
    }

    applyStandbyIdleSetting(static_cast<uint32_t>(idle_seconds) * 1000UL, false);
    applyStandbyBrightnessSetting(brightnessLevelFromWebScale(static_cast<int>(brightness_scale)), false);
    applyStandbyModeSetting(next_mode, false);
    saveSettingsToNVS();
    weather_config_server.send(200, "text/html; charset=utf-8", renderAdminPage("Standby settings saved.", false, "", nullptr, 0, "", false, "standby"));
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

void handleDeviceReboot()
{
    const String section = weather_config_server.arg("section");
    scheduleReboot();
    weather_config_server.send(
        200,
        "text/html; charset=utf-8",
        renderAdminPage("Reboot scheduled. The device will restart in a moment.", false, "", nullptr, 0, "", false, section));
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

// ---------------------------------------------------------------------------
// Admin server lifecycle
// ---------------------------------------------------------------------------

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
    weather_config_server.on("/settings/standby", HTTP_POST, handleStandbySettingsSave);
    weather_config_server.on("/settings/time", HTTP_POST, handleTimezoneSave);
    weather_config_server.on("/device/save", HTTP_POST, handleDeviceConfigSave);
    weather_config_server.on("/wifi/save", HTTP_POST, handleWifiConfigSave);
    weather_config_server.on("/wifi/reset", HTTP_POST, handleWifiReset);
    weather_config_server.on("/device/reboot", HTTP_POST, handleDeviceReboot);
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

// ---------------------------------------------------------------------------
// Provisioning server handlers
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Provisioning server lifecycle
// ---------------------------------------------------------------------------

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
