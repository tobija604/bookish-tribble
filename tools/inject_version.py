"""
PlatformIO pre-build script: stamps the build with a build timestamp and,
when available, the current git commit hash, so ABOUT / DIAGNOSTICS can show
a precise build identifier (spec section 50, "Firmware: vX.X.X").
"""
import subprocess
import datetime

Import("env")  # noqa: F821  (provided by PlatformIO at build time)

def get_git_hash():
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], stderr=subprocess.DEVNULL
        )
        return out.decode().strip()
    except Exception:
        return "nogit"

build_time = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")
git_hash = get_git_hash()

env.Append(  # noqa: F821
    BUILD_FLAGS=[
        f'-D SMARTBOX_BUILD_TIME=\\"{build_time}\\"',
        f'-D SMARTBOX_GIT_HASH=\\"{git_hash}\\"',
    ]
)
