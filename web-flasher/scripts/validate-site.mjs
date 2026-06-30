import assert from "node:assert/strict";
import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import process from "node:process";

const outputDirectory = path.resolve(process.argv[2] || "dist");
const requiredFiles = [
  "index.html",
  "guide/index.html",
  "guide/screenshots/README.md",
  "firmware-index.json",
];

for (const relativePath of requiredFiles) {
  const filePath = path.join(outputDirectory, relativePath);
  assert.ok((await stat(filePath)).size > 0, `Missing or empty site file: ${relativePath}`);
}

const installerHtml = await readFile(path.join(outputDirectory, "index.html"), "utf8");
const guideHtml = await readFile(path.join(outputDirectory, "guide/index.html"), "utf8");
assert.match(installerHtml, /HOMEsmthng Installer/);
assert.match(installerHtml, /Setup guide/);
assert.match(guideHtml, /HOMEsmthng Setup Guide/);
assert.match(guideHtml, /Open browser installer/);

console.log("Validated installer and beginner guide output.");
