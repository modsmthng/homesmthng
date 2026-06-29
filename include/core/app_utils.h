#pragma once

// Small parsing and formatting helpers shared by web, settings and weather code.

#include <Arduino.h>

bool extractJsonNumberField(const String &json, const char *key, float &value);
bool extractJsonStringField(const String &json, const char *key, String &value);
bool parseFloatParameter(const String &text, float &value);
String urlEncode(const String &text);
String htmlEscape(const String &text);
String jsonEscape(const String &text);
int brightnessScaleForWeb(int brightness);
int brightnessLevelFromWebScale(int scale_value);
int normalizeDisplayRotationDegrees(int degrees);
bool parseHexColor(const String &text, uint32_t &color_hex);
String colorHexCss(uint32_t color_hex);
