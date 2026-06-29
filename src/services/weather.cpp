#include "weather.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include "app_utils.h"
#include "settings_store.h"

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
