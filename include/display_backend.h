#pragma once

#include <memory>
#include <stdint.h>

#include "board_profile.h"

class DisplayBackend {
  public:
    virtual ~DisplayBackend() = default;

    virtual bool begin() = 0;
    virtual void setBrightness(uint8_t level) = 0;
    virtual void setUiRotation(uint16_t degrees) = 0;
    virtual const BoardProfile &profile() const = 0;
};

std::unique_ptr<DisplayBackend> createDisplayBackend();
