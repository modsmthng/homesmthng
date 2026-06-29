export const FLASH_MODE = Object.freeze({
  UPDATE: "update",
  INSTALL: "install",
});

export const EXPECTED_PARTS = Object.freeze([
  { name: "bootloader", address: 0x0000 },
  { name: "partitions", address: 0x8000 },
  { name: "boot_app0", address: 0xe000 },
  { name: "firmware", address: 0x10000 },
]);

const SHA256_PATTERN = /^[a-f0-9]{64}$/;

export function isSupportedEnvironment(serial, isSecureContext) {
  return Boolean(serial && isSecureContext);
}

export function isSupportedChip(chipName) {
  return typeof chipName === "string" && /ESP32[- ]?S3/i.test(chipName);
}

export function canStartFlash(boardId, mode) {
  return Boolean(boardId) && Object.values(FLASH_MODE).includes(mode);
}

export function requiresEraseConfirmation(mode) {
  return mode === FLASH_MODE.INSTALL;
}

export function validateFirmwareIndex(index) {
  if (!index || index.schemaVersion !== 1) {
    throw new Error("Unsupported firmware index format.");
  }
  if (typeof index.version !== "string" || !index.version.trim()) {
    throw new Error("The firmware version is missing.");
  }
  if (index.chipFamily !== "ESP32-S3") {
    throw new Error("The firmware index does not target ESP32-S3.");
  }
  if (!Array.isArray(index.boards) || index.boards.length === 0) {
    throw new Error("No display firmware is available.");
  }

  const boardIds = new Set();
  for (const board of index.boards) {
    if (!board || typeof board.id !== "string" || typeof board.label !== "string") {
      throw new Error("A display entry is incomplete.");
    }
    if (boardIds.has(board.id)) {
      throw new Error(`Duplicate display id: ${board.id}`);
    }
    boardIds.add(board.id);

    if (!Array.isArray(board.parts) || board.parts.length !== EXPECTED_PARTS.length) {
      throw new Error(`Firmware parts are incomplete for ${board.label}.`);
    }

    EXPECTED_PARTS.forEach((expected, indexPosition) => {
      const part = board.parts[indexPosition];
      if (part.name !== expected.name || part.address !== expected.address) {
        throw new Error(`Unexpected ${expected.name} address for ${board.label}.`);
      }
      if (typeof part.path !== "string" || !part.path.trim()) {
        throw new Error(`Missing ${expected.name} file for ${board.label}.`);
      }
      if (!SHA256_PATTERN.test(part.sha256)) {
        throw new Error(`Invalid ${expected.name} checksum for ${board.label}.`);
      }
    });
  }

  return index;
}

export function createFlashOptions(files, mode, reportProgress) {
  if (!Array.isArray(files) || files.length !== EXPECTED_PARTS.length) {
    throw new Error("Four verified firmware parts are required.");
  }
  if (!Object.values(FLASH_MODE).includes(mode)) {
    throw new Error("Unknown flashing mode.");
  }

  return {
    fileArray: files.map((file) => ({
      data: file.data,
      address: file.address,
    })),
    flashMode: "keep",
    flashFreq: "keep",
    flashSize: "keep",
    eraseAll: mode === FLASH_MODE.INSTALL,
    compress: true,
    reportProgress,
  };
}

export function createAggregateProgress(fileCount, callback) {
  const progressByFile = Array.from({ length: fileCount }, () => ({ written: 0, total: 1 }));

  return (fileIndex, written, total) => {
    if (fileIndex < 0 || fileIndex >= progressByFile.length || total <= 0) {
      return;
    }
    progressByFile[fileIndex] = { written: Math.min(written, total), total };
    const writtenTotal = progressByFile.reduce((sum, item) => sum + item.written, 0);
    const byteTotal = progressByFile.reduce((sum, item) => sum + item.total, 0);
    callback(byteTotal > 0 ? writtenTotal / byteTotal : 0);
  };
}

export async function sha256Hex(bytes, cryptoProvider = globalThis.crypto) {
  if (!cryptoProvider?.subtle) {
    throw new Error("SHA-256 verification is not available in this browser.");
  }
  const digest = await cryptoProvider.subtle.digest("SHA-256", bytes);
  return Array.from(new Uint8Array(digest), (byte) => byte.toString(16).padStart(2, "0")).join("");
}

export async function downloadAndVerifyParts(board, indexUrl, onPart, fetchProvider = fetch) {
  const files = [];
  for (let partIndex = 0; partIndex < board.parts.length; partIndex += 1) {
    const part = board.parts[partIndex];
    onPart?.(part, partIndex, board.parts.length);
    const fileUrl = new URL(part.path, indexUrl);
    const response = await fetchProvider(fileUrl, { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`Could not download ${part.name} (${response.status}).`);
    }
    const data = new Uint8Array(await response.arrayBuffer());
    const actualChecksum = await sha256Hex(data);
    if (actualChecksum !== part.sha256) {
      throw new Error(`Checksum verification failed for ${part.name}.`);
    }
    files.push({
      name: part.name,
      address: part.address,
      data,
    });
  }
  return files;
}
