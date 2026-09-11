"""Fetch a pinned slangc toolchain into external/slang.

Slang is consumed as a prebuilt binary rather than a submodule: we only need
slangc.exe and its two DLLs, and building Slang from source pulls in its own
LLVM/glslang tree, which costs far more than it is worth here.

The version is pinned so a shader that compiles on one machine compiles the
same way on another. Bump SLANG_VERSION deliberately; the stamp file makes the
download a no-op until you do.
"""

import io
import os
import sys
import zipfile
import urllib.request
import urllib.error

from tqdm import tqdm

import console_colors as color

# Pinned deliberately. Newer Slang releases land roughly monthly; bump this
# when you actually want the new language features, not implicitly.
SLANG_VERSION = "2026.17"

# Slang publishes per-arch archives; this project is Windows x64 only.
SLANG_PLATFORM = "windows-x86_64"

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(TOOLS_DIR)
INSTALL_DIR = os.path.join(ROOT_DIR, "external", "slang")
STAMP_PATH = os.path.join(INSTALL_DIR, ".version")

ARCHIVE_NAME = f"slang-{SLANG_VERSION}-{SLANG_PLATFORM}.zip"
DOWNLOAD_URL = (
    "https://github.com/shader-slang/slang/releases/download/"
    f"v{SLANG_VERSION}/{ARCHIVE_NAME}"
)

# slangc.exe will not start without these next to it, so their absence means a
# half-extracted install even if the stamp file says otherwise.
REQUIRED_FILES = [
    os.path.join("bin", "slangc.exe"),
    os.path.join("bin", "slang.dll"),
    os.path.join("bin", "slang-glslang.dll"),
]


def installed_version():
    """Return the version currently unpacked, or None if it is absent/broken."""
    if not os.path.isfile(STAMP_PATH):
        return None

    for relative in REQUIRED_FILES:
        if not os.path.isfile(os.path.join(INSTALL_DIR, relative)):
            color.warn("external/slang is incomplete, re-downloading")
            return None

    with open(STAMP_PATH, "r", encoding="utf-8") as stamp:
        return stamp.read().strip()


def download(url):
    """Download to memory with a progress bar, returning the raw bytes."""
    color.detail(url)

    try:
        response = urllib.request.urlopen(url)
    except urllib.error.HTTPError as exception:
        color.error(f"download failed: HTTP {exception.code} for {url}")
        return None
    except urllib.error.URLError as exception:
        color.error(f"download failed: {exception.reason}")
        return None

    # Content-Length is advisory; fall back to an indeterminate bar without it.
    total = int(response.headers.get("Content-Length") or 0)
    chunks = []

    with response, tqdm(total=total or None, desc=f"downloading slang {SLANG_VERSION}",
                        unit="B", unit_scale=True, unit_divisor=1024, ncols=100,
                        ascii=True, file=sys.stdout, colour=color.BAR_COLOR) as bar:
        while True:
            chunk = response.read(256 * 1024)
            if not chunk:
                break
            chunks.append(chunk)
            bar.update(len(chunk))

    return b"".join(chunks)


def extract(payload):
    """Unpack the release archive over INSTALL_DIR."""
    try:
        archive = zipfile.ZipFile(io.BytesIO(payload))
    except zipfile.BadZipFile:
        color.error("downloaded file is not a valid zip archive")
        return False

    with archive:
        members = archive.namelist()
        with tqdm(total=len(members), desc="extracting", unit="file", ncols=100,
                  ascii=True, file=sys.stdout, colour=color.BAR_COLOR) as bar:
            for member in members:
                # Guard against archive entries that escape the target directory.
                destination = os.path.realpath(os.path.join(INSTALL_DIR, member))
                if not destination.startswith(os.path.realpath(INSTALL_DIR) + os.sep):
                    color.error(f"refusing to extract outside external/slang: {member}")
                    return False
                archive.extract(member, INSTALL_DIR)
                bar.update(1)

    return True


def main():
    requested = os.environ.get("SLANG_VERSION", SLANG_VERSION)
    if requested != SLANG_VERSION:
        color.warn(f"SLANG_VERSION={requested} overrides the pinned {SLANG_VERSION}")

    current = installed_version()
    if current == SLANG_VERSION:
        color.detail(f"slang {SLANG_VERSION} already present, skipping download")
        return 0

    if current is not None:
        color.detail(f"replacing slang {current} with {SLANG_VERSION}")

    color.step(f"fetching slang {SLANG_VERSION} ({SLANG_PLATFORM})")

    payload = download(DOWNLOAD_URL)
    if payload is None:
        return 1

    os.makedirs(INSTALL_DIR, exist_ok=True)
    if not extract(payload):
        return 1

    missing = [f for f in REQUIRED_FILES
               if not os.path.isfile(os.path.join(INSTALL_DIR, f))]
    if missing:
        color.error(f"archive did not contain: {', '.join(missing)}")
        return 1

    # Written last so an interrupted extract does not look like a good install.
    with open(STAMP_PATH, "w", encoding="utf-8") as stamp:
        stamp.write(SLANG_VERSION + "\n")

    color.success(f"slang {SLANG_VERSION} installed to external/slang")
    return 0


if __name__ == "__main__":
    sys.exit(main())
