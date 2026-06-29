import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import vm from "node:vm";

const webPages = await readFile(
  new URL("../../include/services/web_pages.h", import.meta.url),
  "utf8",
);
const scriptMatch = webPages.match(
  /static const char ADMIN_UPDATE_JS\[\][\s\S]*?R"js\(\s*<script>([\s\S]*?)<\/script>\)js";/,
);
assert.ok(scriptMatch, "ADMIN_UPDATE_JS must remain available for browser tests");

function loadAdminUpdateScript({ currentVersion = "v1.0.0", fetchProvider } = {}) {
  const status = {
    dataset: { currentVersion },
    textContent: "Checking…",
  };
  const context = vm.createContext({
    document: {
      addEventListener() {},
      getElementById(id) {
        return id === "firmware-update-status" ? status : null;
      },
    },
    fetch: fetchProvider || (async () => ({
      ok: true,
      async json() {
        return { version: "v1.0.0" };
      },
    })),
  });
  vm.runInContext(scriptMatch[1], context);
  return { context, status };
}

test("admin update comparison follows Semantic Versioning", () => {
  const { context } = loadAdminUpdateScript();
  assert.equal(context.compareSemanticVersions("v1.2.3", "1.2.4"), -1);
  assert.equal(context.compareSemanticVersions("1.2.3", "v1.2.3"), 0);
  assert.equal(context.compareSemanticVersions("v2.0.0", "v1.9.9"), 1);
  assert.equal(context.compareSemanticVersions("v1.0.0-beta.2", "v1.0.0-beta.11"), -1);
  assert.equal(context.compareSemanticVersions("v1.0.0-rc.1", "v1.0.0"), -1);
  assert.equal(context.compareSemanticVersions("development", "v1.0.0"), null);
  assert.equal(context.compareSemanticVersions("v1.0.0-01", "v1.0.0"), null);
});

test("admin update check reports available, current and development versions", async () => {
  for (const [currentVersion, latestVersion, expected] of [
    ["v1.0.0", "v1.1.0", "v1.1.0 available"],
    ["v1.1.0", "v1.1.0", "Up to date"],
    ["v1.2.0", "v1.1.0", "Newer than published release"],
    ["development", "v1.1.0", "Latest release: v1.1.0"],
  ]) {
    const { context, status } = loadAdminUpdateScript({
      currentVersion,
      fetchProvider: async () => ({
        ok: true,
        async json() {
          return { version: latestVersion };
        },
      }),
    });
    await context.checkForFirmwareUpdate();
    assert.equal(status.textContent, expected);
  }
});

test("admin update check fails without hiding the updater link", async () => {
  const { context, status } = loadAdminUpdateScript({
    fetchProvider: async () => {
      throw new Error("offline");
    },
  });
  await context.checkForFirmwareUpdate();
  assert.equal(status.textContent, "Check unavailable");
});
