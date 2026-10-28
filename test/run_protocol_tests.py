"""Build and run the native protocol suite without attached hardware."""

import argparse
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", nargs="+", default=["c++"])
    parser.add_argument(
        "--sanitize",
        action="store_true",
        help="build with AddressSanitizer and UndefinedBehaviorSanitizer",
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = root / ".pio" / "protocol-tests"
    build.mkdir(parents=True, exist_ok=True)
    executable_name = "protocol-tests-sanitized" if args.sanitize else "protocol-tests"
    executable = build / (f"{executable_name}.exe" if sys.platform == "win32" else executable_name)
    sources = [str(path.relative_to(root)) for path in sorted((root / "src").rglob("*.cpp"))]
    compiler_flags = ["-std=c++20", "-O0", "-g", "-UNDEBUG"]
    if args.sanitize:
        compiler_flags.extend(
            ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        )
    subprocess.run(
        [
            *args.compiler,
            *compiler_flags,
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
