#!/usr/bin/env python3
"""Configure and build Lexico through CMake on Windows, Linux, or macOS."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent

    parser = argparse.ArgumentParser(description="Build Lexico with CMake")
    parser.add_argument(
        "--build-dir",
        type=pathlib.Path,
        default=root / "build-cmake",
        help="CMake build directory (default: build-cmake)",
    )
    parser.add_argument(
        "--config",
        help="Configuration for multi-config generators, such as Debug or Release",
    )
    parser.add_argument(
        "--generator",
        help="Optional CMake generator name",
    )
    args = parser.parse_args()

    build_dir = args.build_dir if args.build_dir.is_absolute() else root / args.build_dir
    configure = ["cmake", "-S", str(root), "-B", str(build_dir)]
    if args.generator:
        configure.extend(["-G", args.generator])

    subprocess.run(configure, check=True)

    build = ["cmake", "--build", str(build_dir), "--target", "lexico"]
    if args.config:
        build.extend(["--config", args.config])
    subprocess.run(build, check=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except FileNotFoundError as exc:
        print(f"error: required executable was not found: {exc.filename}", file=sys.stderr)
        raise SystemExit(1)