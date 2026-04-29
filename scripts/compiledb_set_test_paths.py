from pathlib import Path

Import("env")  # type: ignore  # noqa: F821

project_dir = Path(env["PROJECT_DIR"])  # type: ignore  # noqa: F821
env.Replace(COMPILATIONDB_PATH=str(project_dir / "test" / "compile_commands.json"))  # type: ignore  # noqa: F821
