import { cpSync, existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";

import { defineConfig } from "vite";

const rootDirectory = fileURLToPath(new URL(".", import.meta.url));
const outputDirectory = path.join(rootDirectory, "dist");
const releaseDirectory = process.env.FLASHER_PUBLIC_DIR;

function overlayReleaseAssets() {
  return {
    name: "overlay-release-assets",
    closeBundle() {
      if (releaseDirectory && existsSync(releaseDirectory)) {
        cpSync(releaseDirectory, outputDirectory, { recursive: true, force: true });
      }
    },
  };
}

export default defineConfig({
  base: "./",
  publicDir: "public",
  plugins: [overlayReleaseAssets()],
  build: {
    outDir: "dist",
    emptyOutDir: true,
    rollupOptions: {
      input: {
        installer: path.join(rootDirectory, "index.html"),
        guide: path.join(rootDirectory, "guide/index.html"),
      },
    },
  },
});
