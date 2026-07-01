import { mkdtemp, rm } from "node:fs/promises";
import { spawnSync } from "node:child_process";
import os from "node:os";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const rootDirectory = fileURLToPath(new URL("..", import.meta.url));
const viteEntry = path.join(rootDirectory, "node_modules/vite/bin/vite.js");
const validator = path.join(rootDirectory, "scripts/validate-site.mjs");
const fixtureDirectory = path.join(rootDirectory, "tests/fixtures/legal-content");
const temporaryDirectory = await mkdtemp(path.join(os.tmpdir(), "homesmthng-site-modes-"));

function run(command, args, env) {
  const result = spawnSync(command, args, {
    cwd: rootDirectory,
    env,
    encoding: "utf8",
  });
  if (result.status !== 0) {
    process.stderr.write(result.stdout);
    process.stderr.write(result.stderr);
    throw new Error(`Command failed: ${command} ${args.join(" ")}`);
  }
}

try {
  for (const [mode, legalContentDirectory] of [
    ["without-legal-content", ""],
    ["with-legal-content", fixtureDirectory],
  ]) {
    const outputDirectory = path.join(temporaryDirectory, mode);
    const env = { ...process.env, LEGAL_CONTENT_DIR: legalContentDirectory };
    run(process.execPath, [viteEntry, "build", "--outDir", outputDirectory], env);
    run(process.execPath, [validator, outputDirectory], env);
  }
} finally {
  await rm(temporaryDirectory, { recursive: true, force: true });
}

