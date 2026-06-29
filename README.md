# HOMEsmthng

HOMEsmthng turns supported ESP32-S3 display boards into a HomeKit smart switch dashboard powered by HomeSpan and LVGL.

The project is built for VS Code with PlatformIO using `pioarduino`, with one shared codebase for multiple round display boards.

Demo video: https://youtu.be/96I17sV4fLk?si=4SN58w6wTHuDt7Tn

## Highlights

- Shared LVGL UI across all supported boards
- HomeKit bridge based on HomeSpan
- Support for 9 switch endpoints
- Board-specific display and touch backends selected at build time
- Shared `466x466` UI canvas across targets
- Display care features such as pixel shift, configurable standby (off, dim, or clock), and timezone selection

## Supported Boards

- `trgb_full_circle`: LilyGo T-RGB 2.1" Full Circle
- `lilygo_amoled_175`: LilyGo T-Display-S3 AMOLED 1.75"
- `waveshare_amoled_175`: Waveshare ESP32-S3 Touch AMOLED 1.75"

- 3D-Prints: [Housings](https://www.printables.com/@dubios42/collections/2861668)

The UI is normalized to a shared `466x466` canvas. On the T-RGB this leaves a `7px` black border around the active UI area.

## Toolchain

- IDE: VS Code with the PlatformIO extension
- Platform: `https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip`
- Framework: Arduino

## Quick Start

1. Open the repository in VS Code with PlatformIO installed.
2. Pick one of the supported environments from `platformio.ini`.
3. Build and upload the firmware.
4. On first boot, join the temporary Wi-Fi access point `HOMEsmthng` if credentials are not stored yet.
5. Add the accessory in Apple's Home app using the pairing code shown on the device.

## Build

Build from the repository root with one of these environments:

```bash
pio run -e trgb_full_circle
pio run -e lilygo_amoled_175
pio run -e waveshare_amoled_175
```

Upload firmware:

```bash
pio run -e trgb_full_circle -t upload
pio run -e lilygo_amoled_175 -t upload
pio run -e waveshare_amoled_175 -t upload
```

Open the serial monitor:

```bash
pio device monitor -b 115200
```

## Browser Installer

The latest published release can be installed from
[modsmthng.github.io/homesmthng](https://modsmthng.github.io/homesmthng/) using
desktop Chrome or Edge and a USB data cable.

- **Update** keeps Wi-Fi credentials, HomeKit pairing, and display settings.
- **Install / Reinstall** erases the complete flash before installing HOMEsmthng.

The display model must be selected manually because all supported boards use an
ESP32-S3 and cannot be distinguished reliably by the browser flasher.

The browser installer is built and deployed automatically when a GitHub Release
is published. Before publishing the first release, configure **Settings → Pages →
Source** to use **GitHub Actions**. Pull requests and pushes to `main` validate
all firmware targets, the installer build, its manifest, and firmware checksums
without deploying the site.

## Project Layout

- `src/main.cpp`: application logic, HomeSpan setup, and LVGL UI
- `src/display_backend.cpp`: board-specific display, touch, and LVGL backend code
- `src/board_profile.cpp`: compile-time board definitions and panel metadata
- `include/`: shared headers and LVGL configuration
- `boards/`: local PlatformIO board definitions
- `pictures/`: hardware reference photos used during bring-up and UI tuning

## Board Notes

### LilyGo T-RGB Full Circle

- Uses the official `LilyGo-T-RGB` backend (`LilyGo_RGBPanel` + `LV_Helper`)
- Keeps display and touch handling inside the vendor library path

### LilyGo AMOLED 1.75

- Implemented against LilyGo's official 1.75" reference hardware
- Uses the `CO5300 + CST9217` path in the current firmware

### Waveshare AMOLED 1.75

- Implemented from Waveshare's official pinout and controller data
- Uses `CO5300` display plus `CST9217` touch

## Dependencies

Firmware dependencies are resolved automatically by PlatformIO from `platformio.ini`.
The pinned Python helper used by the build is listed in `requirements-build.txt`.
If it is missing from PlatformIO's Python environment, the build prints the
exact installation command for that environment.

Main libraries:

- HomeSpan
- lvgl 8.x
- LilyGo T-RGB library for the T-RGB target
- GFX Library for Arduino for AMOLED targets
- SensorLib for AMOLED touch handling

## License

This project is released under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)** license.

- You may share and adapt the code.
- You must give appropriate credit.
- Commercial use is not permitted.
- Derivative work must use the same license.

Full license text: <https://creativecommons.org/licenses/by-nc-sa/4.0/>
