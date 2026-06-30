const link = (label, url = null) => ({ label, url });
const value = (text, action = null) => ({ text, action });

export const guideContent = Object.freeze({
  video: {
    youtubeId: "",
    title: "HOMEsmthng video guide",
  },
  enclosureCollection: "https://www.printables.com/@dubios42/collections/2861668",
  hardwareRows: [
    { label: "Exact model", key: "model" },
    { label: "Display", key: "display" },
    { label: "Buy", key: "purchase" },
    { label: "3D-printable case", key: "case" },
    { label: "Screws", key: "screws" },
    { label: "USB-C power", key: "usbPower" },
    { label: "Battery support", key: "battery" },
    { label: "Battery connector", key: "batteryConnector" },
    { label: "Battery to buy", key: "batteryPurchase" },
    { label: "Alternative power", key: "alternativePower" },
    { label: "Battery status", key: "batteryStatus" },
    { label: "Important", key: "important" },
    { label: "Hardware documentation", key: "documentation" },
  ],
  boards: [
    {
      id: "trgb_full_circle",
      name: 'LilyGo T-RGB 2.1"',
      shortName: "T-RGB",
      image: "./screenshots/hardware-trgb.png",
      imageAlt: 'LilyGo T-RGB 2.1-inch Full Circle display board',
      model: value("2.1-inch Full Circle, H597. Do not select the half-circle or 2.8-inch version."),
      display: value("2.1-inch IPS LCD · 480 × 480 · ST7701S"),
      purchase: value("Choose the 2.1-inch Full Circle [H597] option.", link("Official product page", "https://lilygo.cc/products/t-rgb")),
      case: value("Individual model link will be added here.", link("Browse the case collection", "https://www.printables.com/@dubios42/collections/2861668")),
      screws: value("The final screw size and length depend on the selected case.", link("Screw link coming soon")),
      usbPower: value("Regulated 5 V through USB-C. Use a data-capable cable when installing firmware."),
      battery: value("The HOMEsmthng battery enclosure requires an additional battery/power board. Its exact model is not documented yet.", link("Battery board link coming soon")),
      batteryConnector: value("Battery board connector, polarity, pins and input voltage are still to be confirmed."),
      batteryPurchase: value("Choose a battery only after the battery board and enclosure dimensions are confirmed.", link("Battery link coming soon")),
      alternativePower: value("Use USB-C until the required battery board and its supported input are documented. Do not feed power into an unverified pin."),
      batteryStatus: value("HOMEsmthng can show an approximate battery voltage through the board's GPIO 4 measurement path when a supported setup is connected."),
      important: value("The manufacturer documents no free GPIOs on this board. Do not plan additional GPIO accessories."),
      documentation: value("Pin map, schematic and board limitations.", link("LilyGo T-RGB repository", "https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB")),
    },
    {
      id: "lilygo_amoled_175",
      name: 'LilyGo T-Display-S3 AMOLED 1.75"',
      shortName: "LilyGo AMOLED",
      image: "./screenshots/hardware-lilygo-amoled.png",
      imageAlt: 'LilyGo T-Display-S3 AMOLED 1.75-inch display board',
      model: value("1.75-inch H741-01. Do not select the 1.43-inch, 1.64-inch or half-circle variant."),
      display: value("1.75-inch AMOLED · 466 × 466 · H0175Y003AM"),
      purchase: value("Choose the 1.75-inch [H741-01] option.", link("Official product page", "https://lilygo.cc/products/t-display-s3-amoled-1-64")),
      case: value("Individual model link will be added here.", link("Browse the case collection", "https://www.printables.com/@dubios42/collections/2861668")),
      screws: value("The final screw size and length depend on the selected case.", link("Screw link coming soon")),
      usbPower: value("Regulated 5 V through USB-C. Use a data-capable cable when installing firmware."),
      battery: value("Supports a protected single-cell 3.7 V LiPo battery and charging through the onboard SY6970 power controller."),
      batteryConnector: value("Use the matching two-pin battery harness. Verify + and − against the board markings before connecting it; connector polarity is not universal."),
      batteryPurchase: value("Battery dimensions must fit the selected enclosure. Use a protected 1S / 3.7 V LiPo.", link("Battery link coming soon")),
      alternativePower: value("Use USB-C or the dedicated battery input. Do not feed power into an unverified expansion pin."),
      batteryStatus: value("HOMEsmthng reports approximate percentage, voltage and charging state through the SY6970."),
      important: value("Confirm that the package includes the correct display ribbon and battery harness for the 1.75-inch variant."),
      documentation: value("Board revisions, pin overview and power controller details.", link("LilyGo hardware documentation", "https://wiki.lilygo.cc/products/t-display-series/t-display-s3-amoled-1.75/")),
    },
    {
      id: "waveshare_amoled_175",
      name: 'Waveshare ESP32-S3 Touch AMOLED 1.75"',
      shortName: "Waveshare AMOLED",
      image: "./screenshots/hardware-waveshare-amoled.png",
      imageAlt: 'Waveshare ESP32-S3 Touch AMOLED 1.75-inch display board',
      model: value("ESP32-S3-Touch-AMOLED-1.75 with the round 466 × 466 display."),
      display: value("1.75-inch AMOLED · 466 × 466 · CO5300"),
      purchase: value("Choose the standard 1.75-inch board supported by HOMEsmthng.", link("Official product page", "https://www.waveshare.com/product/esp32-s3-touch-amoled-1.75.htm")),
      case: value("Individual model link will be added here.", link("Browse the case collection", "https://www.printables.com/@dubios42/collections/2861668")),
      screws: value("The final screw size and length depend on the selected case.", link("Screw link coming soon")),
      usbPower: value("Regulated 5 V through USB-C. Use a data-capable cable when installing firmware."),
      battery: value("Supports a protected single-cell 3.7 V LiPo battery and charging through the onboard AXP2101 power controller."),
      batteryConnector: value("MX1.25 2-pin battery connector. Check the BAT label and polarity before connecting; the nearby speaker connector is separate."),
      batteryPurchase: value("Battery dimensions must fit the selected enclosure. Use a protected 1S / 3.7 V LiPo with a matching MX1.25 2-pin plug.", link("Battery link coming soon")),
      alternativePower: value("Use USB-C or the dedicated battery input. Do not feed power into an unverified header pin."),
      batteryStatus: value("HOMEsmthng reports percentage, voltage and charging state through the AXP2101."),
      important: value("Match the battery connector and polarity exactly; similar-looking two-pin connectors can be wired differently."),
      documentation: value("Connector locations, schematic and battery safety notes.", link("Waveshare hardware documentation", "https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75")),
    },
  ],
  displayScreenshots: [
    {
      file: "./screenshots/display-first-setup.png",
      title: "First setup",
      alt: "HOMEsmthng display showing first-setup information",
      description: "On first boot, follow the Wi-Fi instructions shown on the display. Once connected, the same screen provides the local setup address and Apple Home pairing information.",
    },
    {
      file: "./screenshots/display-switches.png",
      title: "Switch pages S1–S8",
      alt: "HOMEsmthng display showing the HomeKit switch grid",
      description: "Tap a tile to switch the matching Apple Home accessory. Swipe horizontally to move between S1–S4 and S5–S8. State changes from Apple Home are reflected on the display.",
    },
    {
      file: "./screenshots/display-b1-clock.png",
      title: "B1 and Clock Button",
      alt: "HOMEsmthng display showing the large B1 and Clock Button pages",
      description: "Swipe vertically for the large B1 control and optional Clock Button page. The order and the five-second backlight behavior can be changed in the Web UI.",
    },
    {
      file: "./screenshots/display-settings-standby.png",
      title: "Settings and standby",
      alt: "HOMEsmthng display showing settings and Clock Saver standby",
      description: "The final display page shows connection and battery status and provides brightness control. Standby can turn the display off, dim it or show Clock Saver after inactivity.",
    },
  ],
  webScreenshots: [
    {
      file: "./screenshots/web-overview.png",
      title: "Overview and firmware",
      alt: "HOMEsmthng Web UI overview card",
      description: "Open the local address shown on the display. The overview lists connection details, the installed firmware version and whether a newer release is available.",
    },
    {
      file: "./screenshots/web-homekit-wifi.png",
      title: "Apple Home and Wi-Fi",
      alt: "HOMEsmthng Web UI Apple Home and Wi-Fi sections",
      description: "Use the Apple Home section for pairing. Device and Wi-Fi sections rename the display, change its local hostname or move it to another 2.4 GHz network.",
    },
    {
      file: "./screenshots/web-appearance-standby.png",
      title: "Appearance and standby",
      alt: "HOMEsmthng Web UI appearance and standby settings",
      description: "Adjust brightness, switch color, pixel shift and B1 page behavior. Standby controls the idle time, mode and dim level without stopping HomeSpan or the interface.",
    },
    {
      file: "./screenshots/web-time-weather-battery.png",
      title: "Time, weather and battery",
      alt: "HOMEsmthng Web UI time, weather and battery sections",
      description: "Select a timezone, choose a weather location and check available battery telemetry. Weather and time require Wi-Fi; battery details depend on the selected board.",
    },
  ],
});

export function isExternalUrl(url) {
  if (typeof url !== "string") {
    return false;
  }
  try {
    return new URL(url).protocol === "https:";
  } catch {
    return false;
  }
}

export function isYouTubeId(valueToCheck) {
  return /^[A-Za-z0-9_-]{11}$/.test(valueToCheck || "");
}
