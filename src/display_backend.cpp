#include "display_backend.h"

#include <Arduino.h>
#include <lvgl.h>

#if defined(HOMESMTHNG_BOARD_TRGB_FULL_CIRCLE)
#include <LilyGo_RGBPanel.h>
#include <LV_Helper.h>
#endif

#if defined(HOMESMTHNG_BOARD_LILYGO_AMOLED_175) || defined(HOMESMTHNG_BOARD_WAVESHARE_AMOLED_175)
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <touch/TouchDrvCST92xx.h>
#endif

namespace {

static uint8_t toPanelBrightness(uint8_t level, int max_levels)
{
    if (level == 0 || max_levels <= 0) {
        return 0;
    }
    return static_cast<uint8_t>((static_cast<uint16_t>(level) * 255U) / static_cast<uint16_t>(max_levels));
}

static lv_disp_rot_t rotationToLvgl(uint16_t degrees)
{
    switch (degrees) {
    case 90:
        return LV_DISP_ROT_90;
    case 180:
        return LV_DISP_ROT_180;
    case 270:
        return LV_DISP_ROT_270;
    default:
        return LV_DISP_ROT_NONE;
    }
}

static void applyLvglRotation(uint16_t degrees)
{
    lv_disp_t *disp = lv_disp_get_default();
    if (!disp || !disp->driver) {
        return;
    }

    disp->driver->sw_rotate = 1;
    lv_disp_set_rotation(disp, rotationToLvgl(degrees));
    lv_obj_invalidate(lv_scr_act());
}

#if defined(HOMESMTHNG_BOARD_TRGB_FULL_CIRCLE)

class TrgbDisplayBackend final : public DisplayBackend {
  public:
    explicit TrgbDisplayBackend(const BoardProfile &profile) : profile_(profile) {}

    bool begin() override
    {
        if (!panel_.begin(LILYGO_T_RGB_2_1_INCHES_FULL_CIRCLE)) {
            return false;
        }

        beginLvglHelper(panel_);
        applyLvglRotation(ui_rotation_degrees_);
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);
        return true;
    }

    void setBrightness(uint8_t level) override
    {
        panel_.setBrightness(level);
    }

    void setUiRotation(uint16_t degrees) override
    {
        ui_rotation_degrees_ = degrees;
        applyLvglRotation(ui_rotation_degrees_);
    }

    const BoardProfile &profile() const override
    {
        return profile_;
    }

  private:
    const BoardProfile &profile_;
    LilyGo_RGBPanel panel_;
    uint16_t ui_rotation_degrees_ = 0;
};

#endif

#if defined(HOMESMTHNG_BOARD_LILYGO_AMOLED_175) || defined(HOMESMTHNG_BOARD_WAVESHARE_AMOLED_175)

class AmoledDisplayBackend;
static AmoledDisplayBackend *g_active_amoled_backend = nullptr;

static lv_color_t *allocateDrawBuffer(size_t pixels)
{
    const size_t bytes = pixels * sizeof(lv_color_t);

    lv_color_t *buffer = static_cast<lv_color_t *>(heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!buffer) {
        buffer = static_cast<lv_color_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    return buffer;
}

class AmoledDisplayBackend final : public DisplayBackend {
  public:
    explicit AmoledDisplayBackend(const BoardProfile &profile) : profile_(profile) {}

    bool begin() override
    {
        g_active_amoled_backend = this;

        if (profile_.display_enable >= 0) {
            pinMode(profile_.display_enable, OUTPUT);
            digitalWrite(profile_.display_enable, HIGH);
        }

        bus_ = new Arduino_ESP32QSPI(
            profile_.qspi_cs,
            profile_.qspi_sclk,
            profile_.qspi_data0,
            profile_.qspi_data1,
            profile_.qspi_data2,
            profile_.qspi_data3);

        switch (profile_.display_backend) {
        case DisplayBackendKind::AmoledCo5300:
            oled_ = new Arduino_CO5300(
                bus_,
                profile_.display_reset,
                0,
                profile_.ui.display_width,
                profile_.ui.display_height,
                profile_.display_col_offset1,
                profile_.display_row_offset1,
                profile_.display_col_offset2,
                profile_.display_row_offset2);
            break;
        case DisplayBackendKind::AmoledSh8601:
            oled_ = new Arduino_SH8601(
                bus_,
                profile_.display_reset,
                0,
                profile_.ui.display_width,
                profile_.ui.display_height,
                profile_.display_col_offset1,
                profile_.display_row_offset1,
                profile_.display_col_offset2,
                profile_.display_row_offset2);
            break;
        default:
            return false;
        }

        gfx_ = oled_;
        if (!gfx_) {
            return false;
        }

        if (profile_.touch_backend == TouchBackendKind::Cst9217) {
            touch_.setPins(profile_.touch_reset, profile_.touch_irq);
            if (!touch_.begin(Wire, profile_.touch_address, profile_.touch_sda, profile_.touch_scl)) {
                return false;
            }
            touch_.setMaxCoordinates(profile_.ui.display_width, profile_.ui.display_height);
            touch_.setSwapXY(profile_.touch_swap_xy);
            touch_.setMirrorXY(profile_.touch_mirror_x, profile_.touch_mirror_y);
            attachInterrupt(profile_.touch_irq, onTouchInterrupt, FALLING);
        }

        if (!gfx_->begin(80000000)) {
            return false;
        }

        gfx_->fillScreen(RGB565_BLACK);
        return initLvgl();
    }

    void setBrightness(uint8_t level) override
    {
        if (oled_) {
            oled_->setBrightness(toPanelBrightness(level, profile_.brightness_levels));
        }
    }

    void setUiRotation(uint16_t degrees) override
    {
        ui_rotation_degrees_ = degrees;
        if (disp_) {
            applyLvglRotation(ui_rotation_degrees_);
        }
    }

    const BoardProfile &profile() const override
    {
        return profile_;
    }

  private:
    static void onTouchInterrupt()
    {
        if (g_active_amoled_backend) {
            g_active_amoled_backend->touch_irq_ = true;
        }
    }

    static void flushCallback(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
    {
        auto *backend = static_cast<AmoledDisplayBackend *>(disp_drv->user_data);
        if (!backend) {
            lv_disp_flush_ready(disp_drv);
            return;
        }
        backend->flush(disp_drv, area, color_p);
    }

    static void rounderCallback(lv_disp_drv_t *disp_drv, lv_area_t *area)
    {
        const lv_coord_t max_x = disp_drv ? (disp_drv->hor_res - 1) : area->x2;
        const lv_coord_t max_y = disp_drv ? (disp_drv->ver_res - 1) : area->y2;

        if (area->x1 > 0 && (area->x1 & 1) != 0) {
            area->x1 -= 1;
        }
        if (area->y1 > 0 && (area->y1 & 1) != 0) {
            area->y1 -= 1;
        }

        if (((area->x2 - area->x1 + 1) & 1) != 0) {
            if (area->x2 < max_x) {
                area->x2 += 1;
            } else if (area->x1 > 0) {
                area->x1 -= 1;
            }
        }
        if (((area->y2 - area->y1 + 1) & 1) != 0) {
            if (area->y2 < max_y) {
                area->y2 += 1;
            } else if (area->y1 > 0) {
                area->y1 -= 1;
            }
        }

        if (area->x1 < 0) {
            area->x1 = 0;
        }
        if (area->y1 < 0) {
            area->y1 = 0;
        }
        if (area->x2 > max_x) {
            area->x2 = max_x;
        }
        if (area->y2 > max_y) {
            area->y2 = max_y;
        }
    }

    static void touchReadCallback(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
    {
        auto *backend = static_cast<AmoledDisplayBackend *>(indev_drv->user_data);
        if (!backend) {
            data->state = LV_INDEV_STATE_REL;
            return;
        }
        backend->readTouch(data);
    }

    bool initLvgl()
    {
        lv_init();

        const size_t draw_pixels = static_cast<size_t>(profile_.ui.display_width) * 40U;
        draw_buffer_1_ = allocateDrawBuffer(draw_pixels);
        draw_buffer_2_ = allocateDrawBuffer(draw_pixels);

        if (!draw_buffer_1_) {
            return false;
        }

        lv_disp_draw_buf_init(&draw_buf_, draw_buffer_1_, draw_buffer_2_, draw_pixels);

        lv_disp_drv_init(&disp_drv_);
        disp_drv_.hor_res = profile_.ui.display_width;
        disp_drv_.ver_res = profile_.ui.display_height;
        disp_drv_.flush_cb = flushCallback;
        disp_drv_.rounder_cb = rounderCallback;
        disp_drv_.draw_buf = &draw_buf_;
        disp_drv_.user_data = this;
        disp_drv_.sw_rotate = 1;
        disp_drv_.rotated = rotationToLvgl(ui_rotation_degrees_);
        disp_ = lv_disp_drv_register(&disp_drv_);

        if (profile_.touch_backend != TouchBackendKind::None) {
            lv_indev_drv_init(&indev_drv_);
            indev_drv_.type = LV_INDEV_TYPE_POINTER;
            indev_drv_.read_cb = touchReadCallback;
            indev_drv_.user_data = this;
            lv_indev_drv_register(&indev_drv_);
        }

        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);
        return true;
    }

    void flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
    {
        const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
        const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);

        if (width == 0 || height == 0) {
            lv_disp_flush_ready(disp_drv);
            return;
        }

#if (LV_COLOR_16_SWAP != 0)
        gfx_->draw16bitBeRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(&color_p->full), width, height);
#else
        gfx_->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(&color_p->full), width, height);
#endif

        lv_disp_flush_ready(disp_drv);
    }

    void readTouch(lv_indev_data_t *data)
    {
        if (profile_.touch_backend != TouchBackendKind::Cst9217) {
            data->state = LV_INDEV_STATE_REL;
            return;
        }

        if (touch_irq_ || touch_pressed_) {
            touch_irq_ = false;

            int16_t x[5] = {0};
            int16_t y[5] = {0};
            const uint8_t touched = touch_.getPoint(x, y, 1);

            if (touched > 0) {
                touch_pressed_ = true;
                touch_x_ = x[0];
                touch_y_ = y[0];
            } else {
                touch_pressed_ = false;
            }
        }

        if (touch_pressed_) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_x_;
            data->point.y = touch_y_;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    }

    const BoardProfile &profile_;
    Arduino_DataBus *bus_ = nullptr;
    Arduino_GFX *gfx_ = nullptr;
    Arduino_OLED *oled_ = nullptr;
    TouchDrvCST92xx touch_;
    volatile bool touch_irq_ = false;
    bool touch_pressed_ = false;
    int16_t touch_x_ = 0;
    int16_t touch_y_ = 0;
    uint16_t ui_rotation_degrees_ = 0;
    lv_color_t *draw_buffer_1_ = nullptr;
    lv_color_t *draw_buffer_2_ = nullptr;
    lv_disp_draw_buf_t draw_buf_ = {};
    lv_disp_drv_t disp_drv_ = {};
    lv_disp_t *disp_ = nullptr;
    lv_indev_drv_t indev_drv_ = {};
};

#endif

} // namespace

std::unique_ptr<DisplayBackend> createDisplayBackend()
{
#if defined(HOMESMTHNG_BOARD_TRGB_FULL_CIRCLE)
    return std::unique_ptr<DisplayBackend>(new TrgbDisplayBackend(getBoardProfile()));
#elif defined(HOMESMTHNG_BOARD_LILYGO_AMOLED_175) || defined(HOMESMTHNG_BOARD_WAVESHARE_AMOLED_175)
    return std::unique_ptr<DisplayBackend>(new AmoledDisplayBackend(getBoardProfile()));
#else
#error "No HOMEsmthng board environment selected."
#endif
}
