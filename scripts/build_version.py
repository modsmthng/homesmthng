Import("env")

import os
import re


SAFE_VERSION = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._+-]*$")

version = os.environ.get("HOMESMTHNG_VERSION", "development").strip()
if not SAFE_VERSION.fullmatch(version):
    raise ValueError(
        "HOMESMTHNG_VERSION must contain only letters, numbers, dots, underscores, plus signs and hyphens."
    )

env.Append(CPPDEFINES=[("HOMESMTHNG_VERSION", f'\\"{version}\\"')])
