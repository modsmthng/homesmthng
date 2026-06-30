import assert from "node:assert/strict";
import test from "node:test";

import { guideContent, isExternalUrl, isYouTubeId } from "../guide/content.js";

const expectedBoardIds = [
  "trgb_full_circle",
  "lilygo_amoled_175",
  "waveshare_amoled_175",
];

const expectedHardwareRows = [
  ["Exact model", "model"],
  ["Display", "display"],
  ["Buy", "purchase"],
  ["3D-printable case", "case"],
  ["USB-C", "usbPower"],
  ["Power via rear pins", "rearPinPower"],
  ["Battery", "battery"],
  ["Official documentation", "documentation"],
];

const expectedScreenshots = [
  "display-first-setup.png",
  "display-switches.png",
  "display-b1-clock.png",
  "display-settings-standby.png",
  "web-overview.png",
  "web-homekit-wifi.png",
  "web-appearance-standby.png",
  "web-time-weather-battery.png",
];

test("guide contains every supported board in firmware order", () => {
  assert.deepEqual(guideContent.boards.map((board) => board.id), expectedBoardIds);
  assert.equal(new Set(expectedBoardIds).size, guideContent.boards.length);
});

test("hardware comparison contains only the eight beginner-facing rows", () => {
  assert.deepEqual(
    guideContent.hardwareRows.map((row) => [row.label, row.key]),
    expectedHardwareRows,
  );
  const removedKeys = [
    "batteryConnector",
    "batteryPurchase",
    "alternativePower",
    "batteryStatus",
    "important",
  ];
  for (const board of guideContent.boards) {
    for (const key of removedKeys) {
      assert.equal(Object.hasOwn(board, key), false, `${board.id} still exposes ${key}`);
    }
  }
});

test("case guidance includes the required screw information", () => {
  const [trgb, lilygo, waveshare] = guideContent.boards;
  for (const board of guideContent.boards) {
    assert.match(board.case.text, /Different housings are available in the smthng display housing collection/);
  }
  assert.match(trgb.case.text, /No screws are required/);
  assert.match(lilygo.case.text, /1\.4 mm × 5 mm screws/);
  assert.match(lilygo.case.text, /do not use M2 screws/);
  assert.match(waveshare.case.text, /short M2 screws/);
  assert.match(waveshare.case.text, /approximately 4 mm long/);
  assert.match(waveshare.case.text, /Verify the length against the case before tightening/);
  for (const board of guideContent.boards) {
    assert.equal(Object.hasOwn(board, "screws"), false);
  }
});

test("rear-pin power guidance always requires the official board documentation", () => {
  for (const board of guideContent.boards) {
    assert.match(board.rearPinPower.text, /pins on the back/);
    assert.match(board.rearPinPower.text, /official documentation/);
    assert.match(board.rearPinPower.text, /correct pins and voltage/);
  }
});

test("USB-C is clearly described as a power source", () => {
  for (const board of guideContent.boards) {
    assert.equal(
      board.usbPower.text,
      "Power the display with 5 V via USB-C. Use a data cable for flashing.",
    );
  }
});

test("published guide links are HTTPS and pending links remain non-clickable", () => {
  let pendingLinks = 0;
  for (const board of guideContent.boards) {
    for (const row of guideContent.hardwareRows) {
      const action = board[row.key].action;
      if (!action) {
        continue;
      }
      if (action.url) {
        assert.equal(isExternalUrl(action.url), true, `${board.id} ${row.key}`);
      } else {
        pendingLinks += 1;
      }
    }
  }
  assert.equal(pendingLinks, 0, "the simplified table should not contain pending links");
});

test("battery guidance contains only the buying essentials", () => {
  const [trgb, lilygo, waveshare] = guideContent.boards;
  assert.match(trgb.battery.text, /Additional hardware is required/);
  assert.match(trgb.battery.text, /LilyGo’s official documentation/);
  assert.match(lilygo.battery.text, /protected 1S \/ 3\.7 V LiPo/);
  assert.match(lilygo.battery.text, /matching two-pin plug/);
  assert.match(waveshare.battery.text, /MX1\.25 2-pin plug/);
  for (const board of guideContent.boards) {
    assert.doesNotMatch(board.battery.text, /SY6970|AXP2101|GPIO/);
  }
});

test("Waveshare purchase guidance recommends the board without its metal enclosure", () => {
  const waveshare = guideContent.boards.find((board) => board.id === "waveshare_amoled_175");
  assert.match(waveshare.purchase.text, /without the metal enclosure/);
  assert.match(waveshare.purchase.text, /glued in place/);
});

test("guide exposes the fixed screenshot drop-in contract", () => {
  const screenshotFiles = [
    ...guideContent.displayScreenshots,
    ...guideContent.webScreenshots,
  ].map((item) => item.file.split("/").pop());
  assert.deepEqual(screenshotFiles, expectedScreenshots);
  for (const screenshot of [...guideContent.displayScreenshots, ...guideContent.webScreenshots]) {
    assert.ok(screenshot.alt.length > 10);
    assert.ok(screenshot.description.length > 20);
  }
});

test("video ID validation accepts only embeddable YouTube IDs", () => {
  assert.equal(isYouTubeId("dQw4w9WgXcQ"), true);
  assert.equal(isYouTubeId(""), false);
  assert.equal(isYouTubeId("https://youtu.be/example"), false);
});
