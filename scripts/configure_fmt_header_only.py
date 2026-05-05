"""
Pre-script: write a library.json into the resolved fmt library dir so PlatformIO
treats fmt as header-only. fmt's upstream `src/fmt.cc` is a C++20 module unit
that does not compile in our pre-C++20 builds; with FMT_HEADER_ONLY=1 nothing
in `src/` is needed.
"""

import json
import os

Import("env")  # type: ignore[name-defined]


def patch_fmt(*_args, **_kwargs):
    libdeps_root = env.subst("$PROJECT_LIBDEPS_DIR")
    pioenv = env.subst("$PIOENV")
    fmt_dir = os.path.join(libdeps_root, pioenv, "fmt")
    if not os.path.isdir(fmt_dir):
        return

    manifest_path = os.path.join(fmt_dir, "library.json")
    desired = {
        "name": "fmt",
        "version": "11.2.0",
        "build": {
            "includeDir": "include",
            "srcFilter": ["-<*>"],
        },
    }
    try:
        with open(manifest_path, "r") as fh:
            current = json.load(fh)
        if current == desired:
            return
    except (FileNotFoundError, ValueError):
        pass

    with open(manifest_path, "w") as fh:
        json.dump(desired, fh, indent=2)
        fh.write("\n")


patch_fmt()
