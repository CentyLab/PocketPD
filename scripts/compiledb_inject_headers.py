"""Post-build script: add project header files to compile_commands.json.

Clangd only resolves flags for files listed in compile_commands.json.
PlatformIO only lists .cpp files, so headers opened standalone in the
editor fall back to .clangd flags and lose all include paths.

This script picks the compile_commands.json entry with the most -I flags
(the application source, which gets all library include paths) and clones
it for every .h file under include/ and lib/.

Runs via atexit so it executes after PlatformIO finishes writing the
compilation database.
"""

import atexit
import json
import shlex
from pathlib import Path

Import("env")  # type: ignore  # noqa: F821

project_dir = Path(env["PROJECT_DIR"])  # type: ignore  # noqa: F821
compdb_path = project_dir / "compile_commands.json"


def _count_includes(entry):
    return entry["command"].count(" -I")


def inject_headers():
    if not compdb_path.exists():
        return

    with open(compdb_path) as f:
        db = json.load(f)

    if not db:
        return

    src_entry = max(db, key=_count_includes)

    existing_files = {e["file"] for e in db}

    header_dirs = [project_dir / "include", project_dir / "lib"]
    headers = []
    for d in header_dirs:
        if d.exists():
            headers.extend(d.rglob("*.h"))

    parts = shlex.split(src_entry["command"])
    output_flag_idx = parts.index("-o") if "-o" in parts else None
    added = 0

    for header in headers:
        hstr = str(header.resolve())
        if hstr in existing_files:
            continue

        new_parts = list(parts)
        new_parts[-1] = hstr
        if output_flag_idx is not None:
            new_parts[output_flag_idx + 1] = hstr + ".o"

        db.append(
            {
                "command": " ".join(shlex.quote(p) for p in new_parts),
                "directory": src_entry["directory"],
                "file": hstr,
            }
        )
        added += 1

    if added:
        with open(compdb_path, "w") as f:
            json.dump(db, f, indent=4)
        print(f"compdb: injected {added} header entries")


atexit.register(inject_headers)
