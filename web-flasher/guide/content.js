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
    { label: "USB-C", key: "usbPower" },
    { label: "Power via rear pins", key: "rearPinPower" },
    { label: "Battery", key: "battery" },
    { label: "Official documentation", key: "documentation" },
  ],
  boards: [
    {
      id: "trgb_full_circle",
      name: 'LilyGo T-RGB 2.1"',
      shortName: "T-RGB",
      image: "./screenshots/hardware-trgb.png",
      imageAlt: 'LilyGo T-RGB 2.1-inch Full Circle display board',
      model: value("2.1-inch Full Circle (H597) — not Half Circle or 2.8-inch."),
      display: value("2.1-inch LCD · 480 × 480"),
      purchase: value("Choose the 2.1-inch Full Circle [H597] option.", link("Official product page", "https://lilygo.cc/products/t-rgb")),
      case: value("Available in the HOMEsmthng case collection. No screws are required for the HOMEsmthng case.", link("Browse the case collection", "https://www.printables.com/@dubios42/collections/2861668")),
      usbPower: value("Power the display with 5 V via USB-C. Use a data cable for flashing."),
      rearPinPower: value("Possible through pins on the back. Check LilyGo’s official documentation for the correct pins and voltage before connecting power."),
      battery: value("Additional hardware is required. Check LilyGo’s official documentation before buying or connecting battery parts."),
      documentation: value("Board and battery details.", link("LilyGo T-RGB repository", "https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB")),
    },
    {
      id: "lilygo_amoled_175",
      name: 'LilyGo T-Display-S3 AMOLED 1.75"',
      shortName: "LilyGo AMOLED",
      image: "./screenshots/hardware-lilygo-amoled.png",
      imageAlt: 'LilyGo T-Display-S3 AMOLED 1.75-inch display board',
      model: value("1.75-inch H741-01 — not 1.43-inch, 1.64-inch or Half Circle."),
      display: value("1.75-inch AMOLED · 466 × 466"),
      purchase: value("Choose the 1.75-inch [H741-01] option.", link("Official product page", "https://lilygo.cc/products/t-display-s3-amoled-1-64")),
      case: value("Available in the HOMEsmthng case collection. Use 1.4 mm × 5 mm screws; do not use M2 screws.", link("Browse the case collection", "https://www.printables.com/@dubios42/collections/2861668")),
      usbPower: value("Power the display with 5 V via USB-C. Use a data cable for flashing."),
      rearPinPower: value("Possible through pins on the back. Check LilyGo’s official documentation for the correct pins and voltage before connecting power."),
      battery: value("Battery-ready. Use a protected 1S / 3.7 V LiPo with the matching two-pin plug. Charging and battery status are built in."),
      documentation: value("Board and battery details.", link("LilyGo hardware documentation", "https://wiki.lilygo.cc/products/t-display-series/t-display-s3-amoled-1.75/")),
    },
    {
      id: "waveshare_amoled_175",
      name: 'Waveshare ESP32-S3 Touch AMOLED 1.75"',
      shortName: "Waveshare AMOLED",
      image: "./screenshots/hardware-waveshare-amoled.png",
      imageAlt: 'Waveshare ESP32-S3 Touch AMOLED 1.75-inch display board',
      model: value("ESP32-S3-Touch-AMOLED-1.75 with the round display."),
      display: value("1.75-inch AMOLED · 466 × 466"),
      purchase: value("For use with a 3D-printed case, choose the version without the metal enclosure. The display supplied with the metal enclosure is glued in place and can be difficult to remove without damage.", link("Official product page", "https://www.waveshare.com/product/esp32-s3-touch-amoled-1.75.htm")),
      case: value("Available in the HOMEsmthng case collection. Use short M2 screws, approximately 4 mm long. Verify the length against the case before tightening.", link("Browse the case collection", "https://www.printables.com/@dubios42/collections/2861668")),
      usbPower: value("Power the display with 5 V via USB-C. Use a data cable for flashing."),
      rearPinPower: value("Possible through pins on the back. Check Waveshare’s official documentation for the correct pins and voltage before connecting power."),
      battery: value("Battery-ready. Use a protected 1S / 3.7 V LiPo with an MX1.25 2-pin plug. Charging and battery status are built in."),
      documentation: value("Board and battery details.", link("Waveshare hardware documentation", "https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75")),
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
