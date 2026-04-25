#include "board_profile.h"

// Compile-time board descriptions.
//
// Centralizing these values lets the UI scale and the backend choose pinouts
// without scattering board-specific conditionals through the app.

namespace {

constexpr BoardProfile kTrgbFullCircle = {
    "trgb_full_circle",
    "LilyGo T-RGB Full Circle",
    "HOMEsmthng Bridge",
    DisplayBackendKind::TrgbPanel,
    TouchBackendKind::Integrated,
    {480, 480, 466, 7, 7},
    16,
    -1, -1, -1, -1, -1, -1,
    -1,
    -1,
    -1, -1, -1, -1,
    0x00,
    0, 0, 0, 0,
    false,
    false,
    false,
};

// LilyGo's official 1.75-inch H0175Y003AM reference uses a CO5300 panel and CST9217 touch.
constexpr BoardProfile kLilyGoAmoled175 = {
    "lilygo_amoled_175",
    "LilyGo AMOLED 1.75",
    "HOMEsmthng Bridge",
    DisplayBackendKind::AmoledCo5300,
    TouchBackendKind::Cst9217,
    {473, 467, 466, 3, 0},
    16,
    10, 12, 11, 13, 14, 15,
    17,
    16,
    7, 6, -1, 9,
    0x5A,
    6, 0, 0, 0,
    false,
    true,
    true,
};

constexpr BoardProfile kWaveshareAmoled175 = {
    "waveshare_amoled_175",
    "Waveshare AMOLED 1.75",
    "HOMEsmthng Bridge",
    DisplayBackendKind::AmoledCo5300,
    TouchBackendKind::Cst9217,
    {466, 466, 466, 0, 0},
    16,
    12, 38, 4, 5, 6, 7,
    39,
    -1,
    15, 14, 40, 11,
    0x5A,
    6, 0, 0, 0,
    false,
    true,
    true,
};

} // namespace

const BoardProfile &getBoardProfile()
{
#if defined(HOMESMTHNG_BOARD_TRGB_FULL_CIRCLE)
    return kTrgbFullCircle;
#elif defined(HOMESMTHNG_BOARD_LILYGO_AMOLED_175)
    return kLilyGoAmoled175;
#elif defined(HOMESMTHNG_BOARD_WAVESHARE_AMOLED_175)
    return kWaveshareAmoled175;
#else
#error "No HOMEsmthng board environment selected."
#endif
}
