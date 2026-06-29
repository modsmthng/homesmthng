from importlib.metadata import PackageNotFoundError, version
import sys


REQUIRED_FATFS_NG_VERSION = "0.1.15"


def _installed_version():
    try:
        return version("fatfs-ng")
    except PackageNotFoundError:
        return None


def _check_imports():
    try:
        from fatfs import (Partition, RamDisk, create_extended_partition,
                           create_esp32_wl_image, calculate_esp32_wl_overhead,
                           is_esp32_wl_image, extract_fat_from_esp32_wl)
        from fatfs.partition_extended import PartitionExtended
        from fatfs.wrapper import pyf_mkfs, PY_FR_OK
        return True
    except ImportError:
        return False


installed_version = _installed_version()
if installed_version != REQUIRED_FATFS_NG_VERSION or not _check_imports():
    raise RuntimeError(
        "fatfs-shim: the pinned build dependency fatfs-ng=="
        f"{REQUIRED_FATFS_NG_VERSION} is required (installed: "
        f"{installed_version or 'none'}). Run:\n"
        f"  {sys.executable} -m pip install -r requirements-build.txt"
    )
