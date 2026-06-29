#include <Arduino.h>
#include <math.h>
#include <lvgl.h>

#include "app_state.h"
#include "app_utils.h"
#include "battery_monitor.h"
#include "clock_ui.h"
#include "settings_store.h"
#include "weather.h"
#include "web_admin.h"

static void hideScrollbarVisuals(lv_obj_t *obj)
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

static void disableDecorationInteraction(lv_obj_t *obj)
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

void updateBatteryStatusLabel()
{
    if (!ui_battery_status_label) {
        return;
    }

    const BatteryStatus &status = batteryMonitor().status();
    String icon = LV_SYMBOL_BATTERY_EMPTY;
    if (status.warning != BatteryWarningKind::None) {
        icon = LV_SYMBOL_WARNING;
    } else if (status.charge_state == BatteryChargeState::Charging || status.charge_state == BatteryChargeState::Charged) {
        icon = LV_SYMBOL_CHARGE;
    } else if (status.percentage >= 80) {
        icon = LV_SYMBOL_BATTERY_FULL;
    } else if (status.percentage >= 55) {
        icon = LV_SYMBOL_BATTERY_3;
    } else if (status.percentage >= 25) {
        icon = LV_SYMBOL_BATTERY_2;
    } else if (status.percentage >= 0) {
        icon = LV_SYMBOL_BATTERY_1;
    }

    const String battery_text =
        icon + String(" ") + batteryStatusPrimaryText(status)
        + String("\n") + batteryStatusSecondaryText(status);
    lv_label_set_text(ui_battery_status_label, battery_text.c_str());
    lv_obj_set_style_text_color(
        ui_battery_status_label,
        (status.warning == BatteryWarningKind::None) ? lv_color_make(210, 210, 210) : lv_palette_main(LV_PALETTE_RED),
        0);
}

static void small_switch_event_handler(lv_event_t *e)
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

static void big_switch_event_handler(lv_event_t *e)
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

    syncScreen2IdleTimer();
    safeSetBrightness(currentBrightnessTarget());
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    applyBrightnessSetting(static_cast<int>(lv_slider_get_value(lv_event_get_target(e))), true);
}

static void color_button_event_handler(lv_event_t *e)
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

static void screen2_backlight_toggle_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    const bool auto_off_enabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    applyScreen2BacklightSetting(!auto_off_enabled, true);
}

static void oled_pixel_shift_toggle_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    applyPixelShiftSetting(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED), true);
}

static void clock_button_toggle_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    applyClockButtonSetting(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED), true);
}

static void timezone_button_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    openTimezoneModal();
}

static void timezone_roller_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    applyTimezoneSettingIndex(static_cast<int>(lv_roller_get_selected(lv_event_get_target(e))), false);
}

static void timezone_done_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    saveSettingsToNVS();
    closeTimezoneModal();
}

static void weather_setup_button_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    openWeatherSetupModal();
}

static void weather_setup_done_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    closeWeatherSetupModal();
}

static void tileview_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    updateScrollbarModes();
    syncScreen2IdleTimer();
    safeSetBrightness(currentBrightnessTarget());

    if (isMasterClockTileActive()) {
        updateMasterClock(true);
    }
}

static lv_obj_t *createSwitchButton(lv_obj_t *parent, uint8_t id, lv_coord_t x, lv_coord_t y, lv_coord_t size)
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
    const uint8_t master_button_row = clock_button_screen_first ? 2 : 1;
    const uint8_t master_clock_row = clock_button_screen_first ? 1 : 2;
    tileMaster = lv_tileview_add_tile(tileview, 0, master_button_row, LV_DIR_TOP | LV_DIR_BOTTOM);
    tileMasterClock = lv_tileview_add_tile(tileview, 0, master_clock_row, LV_DIR_TOP | LV_DIR_BOTTOM);
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
    lv_obj_set_size(bl_cont, full_size - scaleUi(120), scaleUi(58));
    lv_obj_align(bl_cont, LV_ALIGN_TOP_MID, 0, scaleUi(292));
    lv_obj_set_style_bg_opa(bl_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bl_cont, 0, 0);
    lv_obj_set_style_pad_all(bl_cont, 0, 0);
    lv_obj_clear_flag(bl_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bl_label = lv_label_create(bl_cont);
    lv_label_set_text(bl_label, "Allow Screen 2 to turn off\nafter 5 seconds");
    lv_obj_set_width(bl_label, full_size - scaleUi(210));
    lv_obj_set_style_text_color(bl_label, lv_color_make(210, 210, 210), 0);
    lv_obj_set_style_text_font(bl_label, &lv_font_montserrat_16, 0);
    lv_obj_align(bl_label, LV_ALIGN_LEFT_MID, 0, 0);

    ui_bl_toggle = lv_switch_create(bl_cont);
    lv_obj_set_size(ui_bl_toggle, scaleUi(50), scaleUi(28));
    lv_obj_align(ui_bl_toggle, LV_ALIGN_RIGHT_MID, 0, 0);
    if (!screen2_bl_always_on) {
        lv_obj_add_state(ui_bl_toggle, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(ui_bl_toggle, screen2_backlight_toggle_event_handler, LV_EVENT_VALUE_CHANGED, nullptr);

    ui_battery_status_label = lv_label_create(tileSett);
    lv_obj_set_width(ui_battery_status_label, full_size - scaleUi(120));
    lv_obj_align(ui_battery_status_label, LV_ALIGN_TOP_MID, 0, scaleUi(354));
    lv_obj_set_style_text_align(ui_battery_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(ui_battery_status_label, scaleUi(3), 0);
    lv_obj_set_style_text_font(ui_battery_status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui_battery_status_label, lv_color_make(210, 210, 210), 0);
    updateBatteryStatusLabel();

    lv_obj_t *lbl_home_status = lv_label_create(tileSett);
    lv_label_set_text(lbl_home_status, "HOMEsmthng");
    lv_obj_set_style_text_font(lbl_home_status, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_home_status, lv_color_white(), 0);
    lv_obj_align(lbl_home_status, LV_ALIGN_TOP_MID, 0, scaleUi(412));

    ui_wifi_label = lv_label_create(tileSett);
    lv_label_set_text(ui_wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(ui_wifi_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui_wifi_label, lv_color_white(), 0);
    lv_obj_align(ui_wifi_label, LV_ALIGN_BOTTOM_MID, 0, -scaleUi(8));

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
