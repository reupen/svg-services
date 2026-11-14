#!/usr/bin/env python3.14
import argparse
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("configuration", type=str.lower, choices=["debug", "release"])
parser.add_argument("platform", type=str.lower, choices=["win32", "x64"])


TOOLCHAIN = "stable-x86_64-pc-windows-msvc"
PLATFORM_TO_TARGET_MAP = {
    "win32": "i686-pc-windows-msvc",
    "x64": "x86_64-pc-windows-msvc",
}


def build(configuration, platform):
    target = PLATFORM_TO_TARGET_MAP[platform]

    subprocess.run(
        [
            "cargo",
            f"+{TOOLCHAIN}",
            "build",
            "--target",
            target,
            "--manifest-path",
            r"resvg\crates\c-api\Cargo.toml",
            *(["--release"] if configuration == "release" else []),
            "--target-dir",
            rf"resvg\crates\c-api\{platform}",
        ],
        check=True,
    )

    target_dir = Path("resvg/crates/c-api") / platform
    lib_path = target_dir / target / configuration / "resvg.lib"
    lib_path.copy(target_dir / configuration / "resvg.lib")


def main():
    args = parser.parse_args()
    build(args.configuration, args.platform)


if __name__ == "__main__":
    main()
