#!/usr/bin/env python3.11
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("configuration", type=str.lower, choices=["debug", "release"])
parser.add_argument("platform", type=str.lower, choices=["win32", "x64"])



PLATFORM_TO_TOOLCHAIN_MAP = {
    "win32": "stable-i686-pc-windows-msvc",
    "x64": "stable-x86_64-pc-windows-msvc",
}


def build(configuration, platform):
    toolchain = PLATFORM_TO_TOOLCHAIN_MAP[platform]

    subprocess.run(
        [
            "cargo",
            f"+{toolchain}",
            "build",
            "--manifest-path", r"resvg\crates\c-api\Cargo.toml",
            *(["--release"] if configuration == "release" else []),
            "--target-dir",
            rf"resvg\crates\c-api\{platform}"
        ],
        check=True,
    )


def main():
    args = parser.parse_args()
    build(args.configuration, args.platform)


if __name__ == "__main__":
    main()
