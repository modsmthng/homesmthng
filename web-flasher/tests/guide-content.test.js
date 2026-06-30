import assert from "node:assert/strict";
import test from "node:test";

import { guideContent, isExternalUrl, isYouTubeId } from "../guide/content.js";

const expectedBoardIds = [
  "trgb_full_circle",
  "lilygo_amoled_175",
  "waveshare_amoled_175",
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
  assert.ok(pendingLinks >= 7, "expected maintained shopping-link placeholders");
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
