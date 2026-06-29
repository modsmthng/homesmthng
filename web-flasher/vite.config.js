import { defineConfig } from "vite";

export default defineConfig({
  base: "./",
  publicDir: process.env.FLASHER_PUBLIC_DIR || "public",
  build: {
    outDir: "dist",
    emptyOutDir: true,
  },
});
