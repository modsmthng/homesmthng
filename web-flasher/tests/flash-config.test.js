import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  EXPECTED_PARTS,
  FLASH_MODE,
  canStartFlash,
  createAggregateProgress,
  createFlashOptions,
  isSupportedChip,
  isSupportedEnvironment,
  requiresEraseConfirmation,
  validateFirmwareIndex,
} from "../src/flash-config.js";

const sampleIndex = JSON.parse(
  await readFile(new URL("../public/firmware-index.json", import.meta.url), "utf8"),
);

function verifiedFiles() {
  return EXPECTED_PARTS.map((part) => ({
    ...part,
    data: new Uint8Array([1, 2, 3]),
  }));
}

test("the checked-in development index follows the release schema", () => {
  assert.equal(validateFirmwareIndex(sampleIndex), sampleIndex);
  assert.equal(sampleIndex.boards.length, 3);
});

test("all boards use the required flash addresses", () => {
  for (const board of sampleIndex.boards) {
    assert.deepEqual(
      board.parts.map(({ name, address }) => ({ name, address })),
      EXPECTED_PARTS,
    );
  }
});

test("install erases all flash and update preserves unrelated sectors", () => {
  assert.equal(createFlashOptions(verifiedFiles(), FLASH_MODE.INSTALL).eraseAll, true);
  assert.equal(createFlashOptions(verifiedFiles(), FLASH_MODE.UPDATE).eraseAll, false);
});

test("flash options retain the PlatformIO image headers", () => {
  const options = createFlashOptions(verifiedFiles(), FLASH_MODE.UPDATE);
  assert.equal(options.flashMode, "keep");
  assert.equal(options.flashFreq, "keep");
  assert.equal(options.flashSize, "keep");
  assert.deepEqual(options.fileArray.map(({ address }) => address), EXPECTED_PARTS.map(({ address }) => address));
});

test("a board and a known action are both required", () => {
  assert.equal(canStartFlash("trgb_full_circle", FLASH_MODE.UPDATE), true);
  assert.equal(canStartFlash("", FLASH_MODE.UPDATE), false);
  assert.equal(canStartFlash("trgb_full_circle", ""), false);
  assert.equal(canStartFlash("trgb_full_circle", "erase-something"), false);
});

test("only a clean installation requires destructive confirmation", () => {
  assert.equal(requiresEraseConfirmation(FLASH_MODE.INSTALL), true);
  assert.equal(requiresEraseConfirmation(FLASH_MODE.UPDATE), false);
});

test("only ESP32-S3 devices pass the chip check", () => {
  assert.equal(isSupportedChip("ESP32-S3"), true);
  assert.equal(isSupportedChip("ESP32S3"), true);
  assert.equal(isSupportedChip("ESP32-C3"), false);
  assert.equal(isSupportedChip(undefined), false);
});

test("Web Serial and a secure context are required", () => {
  assert.equal(isSupportedEnvironment({}, true), true);
  assert.equal(isSupportedEnvironment(undefined, true), false);
  assert.equal(isSupportedEnvironment({}, false), false);
});

test("aggregate progress accounts for every firmware part", () => {
  let ratio = 0;
  const report = createAggregateProgress(2, (nextRatio) => {
    ratio = nextRatio;
  });
  report(0, 50, 100);
  assert.equal(ratio, 50 / 101);
  report(1, 100, 100);
  assert.equal(ratio, 150 / 200);
  report(0, 100, 100);
  assert.equal(ratio, 1);
});

test("invalid firmware addresses are rejected", () => {
  const invalidIndex = structuredClone(sampleIndex);
  invalidIndex.boards[0].parts[3].address = 0x20000;
  assert.throws(() => validateFirmwareIndex(invalidIndex), /Unexpected firmware address/);
});
