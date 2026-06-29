#include "battery_monitor.h"

// Board-specific battery sampling is intentionally lightweight.
//
// The settings UI only needs a trustworthy snapshot every few seconds, so we
// prefer conservative state detection over aggressive reporting that could
// mislead users on boards with limited telemetry.

#include "app_state.h"

#include <Wire.h>

#if defined(HOMESMTHNG_BOARD_WAVESHARE_AMOLED_175)
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
#endif

namespace {

constexpr unsigned long kBatteryPollMs = 5000;
constexpr uint8_t kSy6970Address = 0x6A;
constexpr uint8_t kSy6970RegSystemStatus = 0x0B;
constexpr uint8_t kSy6970RegFault = 0x0C;
constexpr uint8_t kSy6970RegBatteryVoltage = 0x0E;
constexpr float kBatteryCurveVoltages[] = {3.30f, 3.50f, 3.60f, 3.70f, 3.75f, 3.80f, 3.85f, 3.90f, 4.00f, 4.10f, 4.20f};
constexpr int kBatteryCurvePercents[] = {0, 5, 12, 25, 40, 55, 68, 78, 90, 97, 100};

#if defined(HOMESMTHNG_BOARD_WAVESHARE_AMOLED_175)
XPowersPMU g_axp2101;
bool g_axp2101_ready = false;
#endif

bool statusesEqual(const BatteryStatus &lhs, const BatteryStatus &rhs)
{
    return lhs.telemetry_available == rhs.telemetry_available
        && lhs.battery_present == rhs.battery_present
        && lhs.approximate == rhs.approximate
        && lhs.percentage == rhs.percentage
        && lhs.voltage_mv == rhs.voltage_mv
        && lhs.charge_state == rhs.charge_state
        && lhs.warning == rhs.warning;
}

int estimatePercentageFromVoltage(uint16_t voltage_mv)
{
    const float voltage = static_cast<float>(voltage_mv) / 1000.0f;
    if (voltage <= kBatteryCurveVoltages[0]) {
        return kBatteryCurvePercents[0];
    }

    constexpr size_t point_count = sizeof(kBatteryCurveVoltages) / sizeof(kBatteryCurveVoltages[0]);
    for (size_t i = 1; i < point_count; ++i) {
        if (voltage <= kBatteryCurveVoltages[i]) {
            const float segment = (voltage - kBatteryCurveVoltages[i - 1]) / (kBatteryCurveVoltages[i] - kBatteryCurveVoltages[i - 1]);
            const float percent =
                static_cast<float>(kBatteryCurvePercents[i - 1])
                + segment * static_cast<float>(kBatteryCurvePercents[i] - kBatteryCurvePercents[i - 1]);
            return static_cast<int>(lroundf(percent));
        }
    }

    return 100;
}

bool readI2cRegister(uint8_t device, uint8_t reg, uint8_t &value)
{
    Wire.beginTransmission(device);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(static_cast<int>(device), 1) != 1) {
        return false;
    }

    value = Wire.read();
    return true;
}

bool readTrgbBatteryStatus(BatteryStatus &status)
{
    if (boardProfile().battery_adc_pin < 0) {
        return false;
    }

    uint32_t total_mv = 0;
    for (int i = 0; i < 8; ++i) {
        total_mv += static_cast<uint32_t>(analogReadMilliVolts(boardProfile().battery_adc_pin)) * 2U;
    }

    const uint16_t voltage_mv = static_cast<uint16_t>(total_mv / 8U);
    status.telemetry_available = true;

    if (voltage_mv < 2800) {
        status.battery_present = false;
        return true;
    }

    status.battery_present = true;
    status.approximate = true;
    status.voltage_mv = voltage_mv;
    status.percentage = estimatePercentageFromVoltage(voltage_mv);

    // T-RGB exposes battery voltage only. USB power can skew the reading, so
    // obviously out-of-range values are reported without a misleading state.
    if (voltage_mv >= 4450) {
        status.charge_state = BatteryChargeState::Unknown;
    } else {
        status.charge_state = BatteryChargeState::OnBattery;
    }

    return true;
}

bool readSy6970BatteryStatus(BatteryStatus &status)
{
    uint8_t system_status = 0;
    uint8_t fault = 0;
    uint8_t battery_voltage_reg = 0;
    if (!readI2cRegister(kSy6970Address, kSy6970RegSystemStatus, system_status)
        || !readI2cRegister(kSy6970Address, kSy6970RegFault, fault)
        || !readI2cRegister(kSy6970Address, kSy6970RegBatteryVoltage, battery_voltage_reg)) {
        status.warning = BatteryWarningKind::ReadFailed;
        return false;
    }

    status.telemetry_available = true;
    const uint8_t battery_voltage_raw = battery_voltage_reg & 0x7F;
    status.battery_present = battery_voltage_raw != 0;

    if (status.battery_present) {
        status.approximate = true;
        status.voltage_mv = static_cast<uint16_t>(2304U + (static_cast<uint16_t>(battery_voltage_raw) * 20U));
        status.percentage = estimatePercentageFromVoltage(status.voltage_mv);

        const uint8_t charge_status = (system_status >> 3) & 0x03;
        switch (charge_status) {
        case 0x01:
        case 0x02:
            status.charge_state = BatteryChargeState::Charging;
            break;
        case 0x03:
            status.charge_state = BatteryChargeState::Charged;
            break;
        default:
            status.charge_state = BatteryChargeState::OnBattery;
            break;
        }
    }

    if ((fault & 0x30) != 0 || (fault & 0x08) != 0) {
        status.warning = BatteryWarningKind::ChargerFault;
    } else if ((fault & 0x07) != 0) {
        status.warning = BatteryWarningKind::ThermalLimit;
    }

    return true;
}

#if defined(HOMESMTHNG_BOARD_WAVESHARE_AMOLED_175)
bool readAxp2101BatteryStatus(BatteryStatus &status)
{
    if (!g_axp2101_ready) {
        return false;
    }

    status.telemetry_available = true;
    status.battery_present = g_axp2101.isBatteryConnect();
    if (!status.battery_present) {
        return true;
    }

    status.voltage_mv = static_cast<uint16_t>(g_axp2101.getBattVoltage());
    const int raw_percent = g_axp2101.getBatteryPercent();
    status.percentage = raw_percent < 0 ? -1 : raw_percent;

    if (g_axp2101.isCharging()) {
        status.charge_state = BatteryChargeState::Charging;
    } else if (status.percentage >= 100) {
        status.charge_state = BatteryChargeState::Charged;
    } else if (g_axp2101.isDischarge()) {
        status.charge_state = BatteryChargeState::OnBattery;
    }

    if (g_axp2101.getThermalRegulationStatus()) {
        status.warning = BatteryWarningKind::ThermalLimit;
    }

    return true;
}
#endif

} // namespace

bool BatteryMonitor::begin()
{
#if defined(HOMESMTHNG_BOARD_WAVESHARE_AMOLED_175)
    g_axp2101_ready = g_axp2101.begin(Wire, AXP2101_SLAVE_ADDRESS, boardProfile().touch_sda, boardProfile().touch_scl);
#endif
    return update(true);
}

bool BatteryMonitor::update(bool force)
{
    const unsigned long now = millis();
    if (!force && (now - last_update_ms_) < kBatteryPollMs) {
        return false;
    }
    last_update_ms_ = now;

    BatteryStatus next_status = {};
    const bool read_ok = readStatus(next_status);
    if (!read_ok) {
        const BatteryWarningKind read_warning =
            next_status.warning == BatteryWarningKind::None
                ? BatteryWarningKind::ReadFailed
                : next_status.warning;
        next_status = status_;
        next_status.warning = read_warning;
    }

    if (statusesEqual(status_, next_status)) {
        return false;
    }

    status_ = next_status;
    return true;
}

const BatteryStatus &BatteryMonitor::status() const
{
    return status_;
}

bool BatteryMonitor::readStatus(BatteryStatus &next_status)
{
    switch (boardProfile().battery_backend) {
    case BatteryBackendKind::TrgbAdc:
        return readTrgbBatteryStatus(next_status);
    case BatteryBackendKind::Sy6970:
        return readSy6970BatteryStatus(next_status);
    case BatteryBackendKind::Axp2101:
#if defined(HOMESMTHNG_BOARD_WAVESHARE_AMOLED_175)
        return readAxp2101BatteryStatus(next_status);
#else
        return false;
#endif
    case BatteryBackendKind::None:
    default:
        break;
    }

    return false;
}

BatteryMonitor &batteryMonitor()
{
    static BatteryMonitor monitor;
    return monitor;
}

String batteryStatusPrimaryText(const BatteryStatus &status)
{
    if (status.warning != BatteryWarningKind::None) {
        return String("Battery warning");
    }
    if (!status.telemetry_available && status.warning == BatteryWarningKind::None) {
        return String("Battery status unavailable");
    }
    if (!status.battery_present) {
        return String("No battery");
    }
    if (status.percentage >= 0) {
        if (status.approximate) {
            return String("Approx. ") + String(status.percentage) + "%";
        }
        return String(status.percentage) + "%";
    }
    if (status.voltage_mv > 0) {
        return String(static_cast<float>(status.voltage_mv) / 1000.0f, 2) + "V";
    }
    return String("Battery detected");
}

String batteryStatusSecondaryText(const BatteryStatus &status)
{
    if (status.warning == BatteryWarningKind::ReadFailed) {
        return String("Status read failed");
    }
    if (status.warning == BatteryWarningKind::ChargerFault) {
        return String("Charger fault");
    }
    if (status.warning == BatteryWarningKind::ThermalLimit) {
        return String("Charging temperature");
    }

    switch (status.charge_state) {
    case BatteryChargeState::Charging:
        return String("Charging");
    case BatteryChargeState::Charged:
        return String("Charged");
    case BatteryChargeState::OnBattery:
        return String("On battery");
    case BatteryChargeState::Unknown:
    default:
        if (status.voltage_mv > 0) {
            return String(static_cast<float>(status.voltage_mv) / 1000.0f, 2) + "V";
        }
        return String("Unknown");
    }
}
