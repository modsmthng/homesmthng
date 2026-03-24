#pragma once

#include <stdint.h>

enum class DisplayBackendKind {
    TrgbPanel,
    AmoledCo5300,
    AmoledSh8601,
};

enum class TouchBackendKind {
    Integrated,
    Cst9217,
    Ft3168,
    None,
};

struct UiMetrics {
    int display_width;
    int display_height;
    int canvas_size;
    int canvas_x;
    int canvas_y;

    int scale(int base_px) const
    {
        return (base_px * canvas_size + 240) / 480;
    }

    int center() const
    {
        return canvas_size / 2;
    }
};

struct BoardProfile {
    const char *id;
    const char *name;
    const char *accessory_name;
    DisplayBackendKind display_backend;
    TouchBackendKind touch_backend;
    UiMetrics ui;
    int brightness_levels;
    int qspi_cs;
    int qspi_sclk;
    int qspi_data0;
    int qspi_data1;
    int qspi_data2;
    int qspi_data3;
    int display_reset;
    int display_enable;
    int touch_sda;
    int touch_scl;
    int touch_reset;
    int touch_irq;
    uint8_t touch_address;
    uint8_t display_col_offset1;
    uint8_t display_row_offset1;
    uint8_t display_col_offset2;
    uint8_t display_row_offset2;
    bool touch_swap_xy;
    bool touch_mirror_x;
    bool touch_mirror_y;
};

const BoardProfile &getBoardProfile();
