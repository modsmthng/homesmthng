import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import process from "node:process";

import { validateFirmwareIndex } from "../src/flash-config.js";

const releaseDirectory = path.resolve(process.argv[2] || "public");
const indexPath = path.join(releaseDirectory, "firmware-index.json");
const index = validateFirmwareIndex(JSON.parse(await readFile(indexPath, "utf8")));

for (const board of index.boards) {
  for (const part of board.parts) {
    const filePath = path.resolve(releaseDirectory, part.path);
    const relativePath = path.relative(releaseDirectory, filePath);
    assert.ok(!relativePath.startsWith("..") && !path.isAbsolute(relativePath), `Unsafe path: ${part.path}`);
    assert.ok((await stat(filePath)).size > 0, `Empty firmware file: ${part.path}`);
    const checksum = createHash("sha256").update(await readFile(filePath)).digest("hex");
    assert.equal(checksum, part.sha256, `Checksum mismatch: ${part.path}`);
  }
}

console.log(`Validated ${index.version}: ${index.boards.length} boards and ${index.boards.length * 4} files.`);
