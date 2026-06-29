#pragma once

#include "app_state.h"

// Clock rendering
void setClockHandPoints(lv_point_t points[2], float angle_deg, lv_coord_t cx, lv_coord_t cy, lv_coord_t length, lv_coord_t tail);
void updateMasterClockAppearance();
void updateMasterClockMode();
void updateScreensaverClock(bool force);
void applyClockAccentColor(lv_color_t color);
bool isBigSwitchOn();

// Clock / standby settings
void applyPixelShiftSetting(bool enabled, bool persist);
void applyClockButtonSetting(bool enabled, bool persist);
void applyStandbyModeSetting(StandbyMode mode, bool persist);
void applyStandbyIdleSetting(uint32_t idle_ms, bool persist);
void applyStandbyBrightnessSetting(int brightness, bool persist);
void refreshStandbyPresentation();
void syncScreen2IdleTimer();

// Standby / OLED protection
void enterStandby();
void exitStandby(bool user_wake);
void applyPixelShiftOffset(int shift_x, int shift_y);
void updateOledProtection();
void screensaver_overlay_event_handler(lv_event_t *e);
