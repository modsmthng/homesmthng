import { cpSync, existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";

import { defineConfig } from "vite";

import {
  copyLegalContent,
  resolveLegalContentDirectory,
  transformLegalLinks,
} from "./scripts/legal-content.mjs";

const rootDirectory = fileURLToPath(new URL(".", import.meta.url));
const releaseDirectory = process.env.FLASHER_PUBLIC_DIR;
const legalContentDirectory = resolveLegalContentDirectory(process.env.LEGAL_CONTENT_DIR);
const legalPagesEnabled = legalContentDirectory !== null;

function overlayReleaseAssets() {
  let outputDirectory;
  return {
    name: "overlay-release-assets",
    configResolved(config) {
      outputDirectory = config.build.outDir;
    },
    closeBundle() {
      if (releaseDirectory && existsSync(releaseDirectory)) {
        cpSync(releaseDirectory, outputDirectory, { recursive: true, force: true });
      }
    },
  };
}

function overlayLegalContent() {
  let outputDirectory;
  return {
    name: "overlay-external-legal-content",
    configResolved(config) {
      outputDirectory = config.build.outDir;
    },
    transformIndexHtml(html) {
      return transformLegalLinks(html, legalPagesEnabled);
    },
    closeBundle() {
      if (legalContentDirectory) {
        copyLegalContent(legalContentDirectory, outputDirectory);
      }
    },
  };
}

export default defineConfig({
  base: "./",
  publicDir: "public",
  define: {
    __HOMESMTHNG_LEGAL_PAGES__: JSON.stringify(legalPagesEnabled),
  },
  plugins: [overlayReleaseAssets(), overlayLegalContent()],
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
