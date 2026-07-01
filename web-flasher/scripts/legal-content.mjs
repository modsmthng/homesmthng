import { cpSync, existsSync, lstatSync, readdirSync } from "node:fs";
import path from "node:path";

export const legalContentEntries = ["legal", "privacy", "legal-assets"];
export const requiredLegalContentFiles = [
  "legal/index.html",
  "privacy/index.html",
  "legal-assets/styles.css",
];

function assertSafeTree(directory) {
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const entryPath = path.join(directory, entry.name);
    const stats = lstatSync(entryPath);
    if (stats.isSymbolicLink()) {
      throw new Error(`Legal content must not contain symbolic links: ${entryPath}`);
    }
    if (stats.isDirectory()) {
      assertSafeTree(entryPath);
    } else if (!stats.isFile()) {
      throw new Error(`Legal content contains an unsupported file type: ${entryPath}`);
    }
  }
}

export function resolveLegalContentDirectory(directory) {
  if (!directory) {
    return null;
  }

  const resolvedDirectory = path.resolve(directory);
  if (!existsSync(resolvedDirectory) || !lstatSync(resolvedDirectory).isDirectory()) {
    throw new Error(`Legal content directory does not exist: ${resolvedDirectory}`);
  }

  const entries = readdirSync(resolvedDirectory, { withFileTypes: true });
  const unexpectedEntries = entries
    .map((entry) => entry.name)
    .filter((name) => !legalContentEntries.includes(name));
  if (unexpectedEntries.length > 0) {
    throw new Error(`Unexpected legal content entries: ${unexpectedEntries.join(", ")}`);
  }

  for (const relativePath of requiredLegalContentFiles) {
    const filePath = path.join(resolvedDirectory, relativePath);
    if (!existsSync(filePath) || !lstatSync(filePath).isFile()) {
      throw new Error(`Missing legal content file: ${relativePath}`);
    }
  }

  assertSafeTree(resolvedDirectory);
  return resolvedDirectory;
}

export function copyLegalContent(directory, outputDirectory) {
  for (const entry of legalContentEntries) {
    cpSync(path.join(directory, entry), path.join(outputDirectory, entry), {
      recursive: true,
      force: true,
    });
  }
}

export function transformLegalLinks(html, enabled) {
  const blockPattern = /\s*<!-- legal-links:start -->[\s\S]*?<!-- legal-links:end -->/g;
  if (!enabled) {
    return html.replace(blockPattern, "");
  }

  return html
    .replace(/\s*<!-- legal-links:start -->\s*/g, "\n")
    .replace(/\s*<!-- legal-links:end -->\s*/g, "\n");
}

