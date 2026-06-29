#pragma once

#include <Arduino.h>

#include "app_state.h"

bool fetchCurrentWeather();
int parseWeatherSearchResults(const String &json, WeatherSearchResult results[], int max_results);
bool fetchWeatherSearchResults(
    const String &query,
    WeatherSearchResult results[],
    int max_results,
    int &result_count,
    String &error_message);
void updateWeatherIfNeeded();
void deferWeatherRefresh(uint32_t delay_ms);

WeatherGlyph glyphForWeatherCode(int code, bool is_day);
const char *weatherGlyphLabel(WeatherGlyph glyph);
String weatherSearchDisplayName(const WeatherSearchResult &result);
void setScreensaverWeatherGlyph(WeatherGlyph glyph);
void setMasterClockWeatherGlyph(WeatherGlyph glyph);
void updateWeatherMonogram();

WeatherLocation selectedWeatherLocation();
String selectedWeatherLocationLabel();
void saveCustomWeatherLocation(const String &name, float latitude, float longitude, bool persist);
void resetCustomWeatherLocation(bool persist);
void applyWeatherEnabledSetting(bool enabled, bool persist);
