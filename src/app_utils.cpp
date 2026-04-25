// Small parsing and formatting helpers shared by the extracted modules.

#include "app_utils.h"

#include <math.h>

#include "app_state.h"

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

int normalizeDisplayRotationDegrees(int degrees)
{
    switch (degrees) {
    case 90:
    case 180:
    case 270:
        return degrees;
    default:
        return 0;
    }
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
