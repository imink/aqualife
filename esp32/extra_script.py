import os
import subprocess
from datetime import datetime, timezone

Import("env")


PROJECT_DIR = env.subst("$PROJECT_DIR")
ROOT_DIR = os.path.abspath(os.path.join(PROJECT_DIR, ".."))
CONVERTER = os.path.join(ROOT_DIR, "tools", "convert-sprites.mjs")
BUILD_INFO = os.path.join(PROJECT_DIR, "include", "build_info.h")
APP_VERSION = "1.0.0"


def git_output(*args):
    try:
        return subprocess.check_output(["git", *args], cwd=ROOT_DIR, text=True).strip()
    except subprocess.CalledProcessError:
        return "unknown"


def generate_build_info(source=None, target=None, env=None):
    print("Generating AquaLife build info...")
    git_sha = git_output("rev-parse", "--short", "HEAD")
    build_time = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    content = (
        "#pragma once\n\n"
        f"#define AQUALIFE_VERSION \"{APP_VERSION}\"\n"
        f"#define AQUALIFE_GIT_SHA \"{git_sha}\"\n"
        f"#define AQUALIFE_BUILD_TIME \"{build_time}\"\n"
    )

    os.makedirs(os.path.dirname(BUILD_INFO), exist_ok=True)
    with open(BUILD_INFO, "w", encoding="utf-8") as build_info:
        build_info.write(content)


def build_assets(source=None, target=None, env=None):
    print("Building AquaLife sprite assets...")
    subprocess.check_call(["node", CONVERTER], cwd=ROOT_DIR)


# Ensure generated headers exist before C++ sources are compiled.
generate_build_info()
build_assets()

env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", generate_build_info)
env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", build_assets)
env.AddPreAction("upload", generate_build_info)
env.AddPreAction("upload", build_assets)

env.AddCustomTarget(
    name="build-info",
    dependencies=None,
    actions=[generate_build_info],
    title="Build Info",
    description="Generate esp32/include/build_info.h with version and git metadata",
)

env.AddCustomTarget(
    name="build-assets",
    dependencies=None,
    actions=[build_assets],
    title="Build Assets",
    description="Convert PNG sprites to esp32/include/sprites.h",
)
