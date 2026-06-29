#pragma once

// Compact battery telemetry abstraction for the supported boards.
//
// The display settings screen only needs a stable, board-agnostic snapshot, so
// the hardware-specific probing stays behind this small interface.

#include <Arduino.h>

enum class BatteryChargeState {
    Unknown,
    OnBattery,
    Charging,
    Charged,
};

enum class BatteryWarningKind {
    None,
    ReadFailed,
    ChargerFault,
    ThermalLimit,
};

struct BatteryStatus {
    bool telemetry_available = false;
    bool battery_present = false;
    bool approximate = false;
    int percentage = -1;
    uint16_t voltage_mv = 0;
    BatteryChargeState charge_state = BatteryChargeState::Unknown;
    BatteryWarningKind warning = BatteryWarningKind::None;
};

class BatteryMonitor {
  public:
    bool begin();
    bool update(bool force = false);
    const BatteryStatus &status() const;

  private:
    bool readStatus(BatteryStatus &next_status);

    BatteryStatus status_ = {};
    unsigned long last_update_ms_ = 0;
};

BatteryMonitor &batteryMonitor();
String batteryStatusPrimaryText(const BatteryStatus &status);
String batteryStatusSecondaryText(const BatteryStatus &status);
