import { ESPLoader, Transport } from "esptool-js";

import {
  createAggregateProgress,
  createFlashOptions,
  downloadAndVerifyParts,
  isSupportedChip,
} from "./flash-config.js";

const CONNECT_BAUD_RATE = 460800;

export async function flashDisplay({ board, mode, indexUrl, onStatus, onProgress, onLog }) {
  let transport;
  let resetCompleted = false;

  try {
    onStatus("Select the display's serial port…");
    const port = await navigator.serial.requestPort();
    transport = new Transport(port, true);

    const terminal = {
      clean() {},
      writeLine(value) {
        onLog?.(String(value));
      },
      write(value) {
        onLog?.(String(value));
      },
    };

    onStatus("Connecting to the display…");
    const loader = new ESPLoader({
      transport,
      baudrate: CONNECT_BAUD_RATE,
      terminal,
      debugLogging: false,
    });
    const chipName = await loader.main("default_reset");
    onLog?.(`Detected chip: ${chipName}`);
    if (!isSupportedChip(chipName)) {
      throw new Error(`Unsupported device: ${chipName || "unknown chip"}. Expected an ESP32-S3.`);
    }

    onProgress(0.03);
    const files = await downloadAndVerifyParts(board, indexUrl, (part, partIndex, partCount) => {
      onStatus(`Downloading and checking ${part.name}…`);
      onProgress(0.03 + ((partIndex + 1) / partCount) * 0.07);
    });

    onStatus(mode === "install" ? "Erasing and installing HOMEsmthng…" : "Updating HOMEsmthng…");
    const reportFlashProgress = createAggregateProgress(files.length, (ratio) => {
      onProgress(0.1 + ratio * 0.9);
    });
    await loader.writeFlash(createFlashOptions(files, mode, reportFlashProgress));

    onProgress(1);
    onStatus("Restarting the display…");
    await loader.after("hard_reset");
    resetCompleted = true;
  } finally {
    if (transport) {
      try {
        await transport.disconnect();
      } catch (disconnectError) {
        if (!resetCompleted) {
          onLog?.(`Disconnect warning: ${disconnectError}`);
        }
      }
    }
  }
}
