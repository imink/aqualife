import os
import subprocess

Import("env")


PROJECT_DIR = env.subst("$PROJECT_DIR")
ROOT_DIR = os.path.abspath(os.path.join(PROJECT_DIR, ".."))
CONVERTER = os.path.join(ROOT_DIR, "tools", "convert-sprites.mjs")


def build_assets(source=None, target=None, env=None):
    print("Building AquaLife sprite assets...")
    subprocess.check_call(["node", CONVERTER], cwd=ROOT_DIR)


# Ensure sprites.h exists before C++ sources are compiled.
build_assets()

env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", build_assets)
env.AddPreAction("upload", build_assets)

env.AddCustomTarget(
    name="build-assets",
    dependencies=None,
    actions=[build_assets],
    title="Build Assets",
    description="Convert PNG sprites to esp32/include/sprites.h",
)
