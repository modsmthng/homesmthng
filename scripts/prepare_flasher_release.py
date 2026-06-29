#!/usr/bin/env python3

import argparse
import hashlib
import json
import os
import re
import shutil
from pathlib import Path


BOARDS = (
    {
        "id": "trgb_full_circle",
        "label": 'LilyGo T-RGB 2.1" Full Circle',
        "description": "Round 2.1-inch LCD",
    },
    {
        "id": "lilygo_amoled_175",
        "label": 'LilyGo T-Display-S3 AMOLED 1.75"',
        "description": "Round 1.75-inch AMOLED",
    },
    {
        "id": "waveshare_amoled_175",
        "label": 'Waveshare ESP32-S3 Touch AMOLED 1.75"',
        "description": "Round 1.75-inch AMOLED",
    },
)

PARTS = (
    ("bootloader", "bootloader.bin", 0x0000),
    ("partitions", "partitions.bin", 0x8000),
    ("boot_app0", "boot_app0.bin", 0xE000),
    ("firmware", "firmware.bin", 0x10000),
)

SAFE_VERSION = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file_handle:
        for chunk in iter(lambda: file_handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def default_boot_app0() -> Path:
    core_dir = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    return core_dir / "packages" / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Prepare HOMEsmthng browser-flasher release files.")
    parser.add_argument("--version", required=True, help="Release tag shown by the installer.")
    parser.add_argument("--output", required=True, type=Path, help="Generated public directory.")
    parser.add_argument("--build-root", type=Path, default=Path(".pio/build"))
    parser.add_argument("--boot-app0", type=Path, default=default_boot_app0())
    return parser.parse_args()


def require_file(path: Path, label: str) -> None:
    if not path.is_file() or path.stat().st_size == 0:
        raise FileNotFoundError(f"Missing {label}: {path}")


def main() -> None:
    args = parse_args()
    version = args.version.strip()
    if not SAFE_VERSION.fullmatch(version):
        raise ValueError("Version must contain only letters, numbers, dots, underscores and hyphens.")

    build_root = args.build_root.resolve()
    output_root = args.output.resolve()
    boot_app0 = args.boot_app0.resolve()
    require_file(boot_app0, "boot_app0 image")
    output_root.mkdir(parents=True, exist_ok=True)

    index = {
        "schemaVersion": 1,
        "version": version,
        "chipFamily": "ESP32-S3",
        "partitionLayout": "default_16MB-v1",
        "boards": [],
    }

    for board in BOARDS:
        board_output = output_root / "firmware" / version / board["id"]
        board_output.mkdir(parents=True, exist_ok=True)
        build_dir = build_root / board["id"]
        board_parts = []

        for name, filename, address in PARTS:
            source = boot_app0 if name == "boot_app0" else build_dir / filename
            require_file(source, f"{board['id']} {name}")
            destination = board_output / filename
            shutil.copyfile(source, destination)
            board_parts.append(
                {
                    "name": name,
                    "path": destination.relative_to(output_root).as_posix(),
                    "address": address,
                    "sha256": sha256(destination),
                }
            )

        index["boards"].append({**board, "parts": board_parts})

    index_path = output_root / "firmware-index.json"
    index_path.write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {version} browser-flasher assets in {output_root}")


if __name__ == "__main__":
    main()
