#include "clock_ui.h"
#include "weather.h"
#include "settings_store.h"

#include <math.h>

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

static bool isLargeB1TileActive()
{
    return tileview && tileMaster && lv_tileview_get_tile_act(tileview) == tileMaster;
}

static bool shouldShowStandbyClock()
{
    if (!isStandbyClockMode()) {
        return false;
    }

    lv_obj_t *active_tile = tileview ? lv_tileview_get_tile_act(tileview) : nullptr;
    return active_tile != tileMaster && active_tile != tileMasterClock;
}

static void screen2OffTimerCallback(lv_timer_t *)
{
    screen2_timer = nullptr;
    if (screen2_bl_always_on || !isLargeB1TileActive()) {
        screen2_backlight_timed_out = false;
        return;
    }

    screen2_backlight_timed_out = true;
    safeSetBrightness(currentBrightnessTarget());
}

void syncScreen2IdleTimer()
{
    if (!screen2_bl_always_on && isLargeB1TileActive()) {
        screen2_backlight_timed_out = false;
        if (screen2_timer) {
            lv_timer_reset(screen2_timer);
        } else {
            screen2_timer = lv_timer_create(screen2OffTimerCallback, kScreen2BacklightIdleMs, nullptr);
            if (screen2_timer) {
                lv_timer_set_repeat_count(screen2_timer, 1);
            }
        }
        return;
    }

    if (screen2_timer) {
        lv_timer_del(screen2_timer);
        screen2_timer = nullptr;
    }
    screen2_backlight_timed_out = false;
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

void applyStandbyModeSetting(StandbyMode mode, bool persist)
{
    if (mode == StandbyMode::ClockSaver && !supportsClockSaver()) {
        mode = StandbyMode::Dim;
    }

    standby_mode = mode;
    if (standby_mode == StandbyMode::Disabled) {
        exitStandby(false);
    } else if (standby_active) {
        refreshStandbyPresentation();
    }

    if (persist) {
        saveSettingsToNVS();
    }
}

void applyStandbyIdleSetting(uint32_t idle_ms, bool persist)
{
    standby_idle_ms = constrain(idle_ms, kStandbyMinIdleMs, kStandbyMaxIdleMs);
    if (persist) {
        saveSettingsToNVS();
    }
}

void applyStandbyBrightnessSetting(int brightness, bool persist)
{
    standby_brightness = constrain(brightness, 1, boardProfile().brightness_levels);
    if (standby_active) {
        safeSetBrightness(currentBrightnessTarget());
    }

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
    if (standby_active && shouldShowStandbyClock()) {
        updateScreensaverClock(true);
    }
    updateMasterClock(true);

    if (persist) {
        saveSettingsToNVS();
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

void refreshStandbyPresentation()
{
    if (!screensaver_overlay || !standby_active) {
        return;
    }

    const bool show_clock = shouldShowStandbyClock();
    const bool transparent_overlay = standby_mode == StandbyMode::Dim
                                     || (standby_mode == StandbyMode::ClockSaver && !show_clock);
    lv_obj_set_style_bg_opa(
        screensaver_overlay,
        transparent_overlay ? LV_OPA_TRANSP : LV_OPA_COVER,
        0);

    if (screensaver_clock_layer) {
        if (show_clock) {
            lv_obj_clear_flag(screensaver_clock_layer, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(screensaver_clock_layer, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (show_clock) {
        updateScreensaverClock(true);
    }
    safeSetBrightness(currentBrightnessTarget());
}

void enterStandby()
{
    if (!screensaver_overlay || standby_active || !isStandbyEnabled()) {
        return;
    }

    closeTimezoneModal();
    closeWeatherSetupModal();

    standby_active = true;
    lv_obj_move_foreground(screensaver_overlay);
    lv_obj_clear_flag(screensaver_overlay, LV_OBJ_FLAG_HIDDEN);
    updateScrollbarModes();
    refreshStandbyPresentation();
}

void exitStandby(bool user_wake)
{
    if (!screensaver_overlay || !standby_active) {
        return;
    }

    standby_active = false;
    lv_obj_add_flag(screensaver_overlay, LV_OBJ_FLAG_HIDDEN);
    updateScrollbarModes();

    if (user_wake) {
        lv_disp_trig_activity(nullptr);
        syncScreen2IdleTimer();
    }
    safeSetBrightness(currentBrightnessTarget());
}

void updateOledProtection()
{
    static unsigned long last_shift_update = 0;
    static unsigned long last_clock_update = 0;
    static size_t shift_index = 0;
    if (!supportsPixelShift() && !isStandbyEnabled()) {
        if (standby_active) {
            exitStandby(false);
        }
        applyPixelShiftOffset(0, 0);
        return;
    }

    const unsigned long now = millis();
    const uint32_t inactive_time = lv_disp_get_inactive_time(nullptr);

    if (isStandbyEnabled()) {
        if (!standby_active && inactive_time >= standby_idle_ms) {
            enterStandby();
            last_clock_update = now;
        }
    } else if (standby_active) {
        exitStandby(false);
    }

    if (standby_active && shouldShowStandbyClock()
        && (last_clock_update == 0 || now - last_clock_update >= 1000UL)) {
        updateScreensaverClock(false);
        last_clock_update = now;
    }

    if (isPixelShiftEnabled()) {
        if (last_shift_update == 0) {
            shift_index = 0;
            applyPixelShiftOffset(0, 0);
            last_shift_update = now;
        } else if (now - last_shift_update >= kPixelShiftIntervalMs) {
            shift_index = (shift_index + 1U) % kPixelShiftStepCount;
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

void screensaver_overlay_event_handler(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED) {
        return;
    }

    const bool switch_b1 = standby_active && isMasterClockTileActive();
    lv_event_stop_bubbling(e);
    lv_event_stop_processing(e);
    exitStandby(true);

    if (switch_b1) {
        const bool new_state = !isBigSwitchOn();
        updateLVGLState(8, new_state);
        if (homekit_switches[8]) {
            homekit_switches[8]->on->setVal(new_state);
        }
    }
}
