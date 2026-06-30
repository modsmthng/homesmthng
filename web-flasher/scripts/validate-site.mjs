import assert from "node:assert/strict";
import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import process from "node:process";

const outputDirectory = path.resolve(process.argv[2] || "dist");
const requiredFiles = [
  "index.html",
  "guide/index.html",
  "guide/screenshots/README.md",
  "guide/screenshots/hardware-trgb.png",
  "guide/screenshots/hardware-lilygo-amoled.png",
  "guide/screenshots/hardware-waveshare-amoled.png",
  "guide/screenshots/web-overview.png",
  "guide/screenshots/web-homekit-wifi.png",
  "guide/screenshots/web-appearance-standby.png",
  "guide/screenshots/web-time-weather-battery.png",
  "guide/screenshots/display-first-setup.png",
  "guide/screenshots/display-switches.png",
  "guide/screenshots/display-b1-clock.png",
  "guide/screenshots/display-settings-standby.png",
  "legal/index.html",
  "privacy/index.html",
  "firmware-index.json",
];

for (const relativePath of requiredFiles) {
  const filePath = path.join(outputDirectory, relativePath);
  assert.ok((await stat(filePath)).size > 0, `Missing or empty site file: ${relativePath}`);
}

const installerHtml = await readFile(path.join(outputDirectory, "index.html"), "utf8");
const guideHtml = await readFile(path.join(outputDirectory, "guide/index.html"), "utf8");
const legalHtml = await readFile(path.join(outputDirectory, "legal/index.html"), "utf8");
const privacyHtml = await readFile(path.join(outputDirectory, "privacy/index.html"), "utf8");
assert.match(installerHtml, /HOMEsmthng Installer/);
assert.match(installerHtml, /Setup guide/);
assert.match(installerHtml, /Legal notice/);
assert.match(installerHtml, /Privacy/);
assert.match(guideHtml, /HOMEsmthng Setup Guide/);
assert.match(guideHtml, /Open browser installer/);
assert.match(guideHtml, /Legal notice/);
assert.match(guideHtml, /Privacy/);
assert.match(legalHtml, /Legal notice \(Impressum\)/);
assert.match(legalHtml, /Andr(?:é|&eacute;) Tolksdorf/);
assert.match(privacyHtml, /Privacy policy/);
assert.match(privacyHtml, /GitHub Pages/);

console.log("Validated installer, beginner guide and legal information output.");
