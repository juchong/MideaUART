"""Build and run the native protocol suite without attached hardware."""

import argparse
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", nargs="+", default=["c++"])
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = root / ".pio" / "protocol-tests"
    build.mkdir(parents=True, exist_ok=True)
    executable = build / ("protocol-tests.exe" if sys.platform == "win32" else "protocol-tests")
    sources = [str(path.relative_to(root)) for path in sorted((root / "src").rglob("*.cpp"))]
    subprocess.run(
        [
            *args.compiler,
            "-std=c++20",
            "-O0",
            "-g",
            "-UNDEBUG",
            "-DARDUINO",
            "-DARDUINO_ARCH_ESP8266",
            "-Itest/stubs",
            "-Iinclude",
            "test/protocol_tests.cpp",
            *sources,
            "-o",
            str(executable),
        ],
        cwd=root,
        check=True,
    )
    subprocess.run([str(executable)], cwd=root, check=True)


if __name__ == "__main__":
    main()
