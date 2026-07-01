import assert from "node:assert/strict";
import { access, readFile } from "node:fs/promises";
import test from "node:test";

import {
  resolveLegalContentDirectory,
  transformLegalLinks,
} from "../scripts/legal-content.mjs";

const installerHtml = await readFile(new URL("../index.html", import.meta.url), "utf8");
const guideHtml = await readFile(new URL("../guide/index.html", import.meta.url), "utf8");
const guideScript = await readFile(new URL("../guide/main.js", import.meta.url), "utf8");
const fixtureDirectory = new URL("./fixtures/legal-content/", import.meta.url);

test("public source contains no bundled legal or privacy pages", async () => {
  await assert.rejects(access(new URL("../legal/index.html", import.meta.url)));
  await assert.rejects(access(new URL("../privacy/index.html", import.meta.url)));
});

test("legal links follow the external-content build mode", () => {
  const disabledInstaller = transformLegalLinks(installerHtml, false);
  const disabledGuide = transformLegalLinks(guideHtml, false);
  assert.doesNotMatch(disabledInstaller, /href="\.\/(?:legal|privacy)\/"/);
  assert.doesNotMatch(disabledGuide, /href="\.\.\/(?:legal|privacy)\/"/);

  const enabledInstaller = transformLegalLinks(installerHtml, true);
  const enabledGuide = transformLegalLinks(guideHtml, true);
  assert.match(enabledInstaller, /href="\.\/legal\/"/);
  assert.match(enabledInstaller, /href="\.\/privacy\/"/);
  assert.match(enabledGuide, /href="\.\.\/legal\/"/);
  assert.match(enabledGuide, /href="\.\.\/privacy\/"/);
});

test("external legal content must satisfy the overlay contract", () => {
  assert.equal(resolveLegalContentDirectory(fixtureDirectory.pathname), fixtureDirectory.pathname.replace(/\/$/, ""));
  assert.throws(() => resolveLegalContentDirectory(new URL("./fixtures/", import.meta.url).pathname));
});

test("YouTube remains blocked until the visitor clicks the load button", () => {
  const clickHandler = guideScript.indexOf('button.addEventListener("click"');
  const iframeSource = guideScript.indexOf("https://www.youtube-nocookie.com/embed/");
  assert.ok(clickHandler >= 0);
  assert.ok(iframeSource > clickHandler);
  assert.match(guideScript, /__HOMESMTHNG_LEGAL_PAGES__/);
  assert.match(guideScript, /Privacy information/);
});

test("guide images open in an accessible native dialog", () => {
  assert.match(guideHtml, /<dialog class="image-lightbox" id="image-lightbox"/);
  assert.match(guideHtml, /aria-label="Close enlarged image"/);
  assert.match(guideScript, /media-zoom-button/);
  assert.match(guideScript, /lightbox\.showModal\(\)/);
  assert.match(guideScript, /event\.target === lightbox/);
});

