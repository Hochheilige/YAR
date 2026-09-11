"""Compile the shader directory to SPIR-V.

Two languages live side by side here:

  *.hlsl   -> dxc.exe    (from VULKAN_SDK)
  *.slang  -> slangc.exe (from external/slang, see tools/download_slang.py)

Both emit SPIR-V, which is all the engine consumes: it reflects the binary
with SPIRV-Reflect and cross-compiles it to GLSL with SPIRV-Cross at load
time. So the language a shader is written in is invisible to the renderer,
and shaders can be ported one at a time.

Output is "<original file name>.spv", because the runtime loader appends
".spv" to whatever path the shader load desc was given. A shader named
foo_comp.slang is therefore loaded as "shaders/foo_comp.slang".
"""

import os
import sys
import subprocess
from pathlib import Path

# Stage is inferred from the file name, matching the existing convention.
# Order matters: "pixel" would also match a naive "pix" test, and a file
# called "comp" must not be mistaken for anything else.
STAGE_RULES = [
    (("vert",), "vertex"),
    (("frag", "pixel", "pix"), "pixel"),
    (("comp",), "compute"),
    (("geom",), "geometry"),
]

# dxc and slangc spell the same stage differently.
DXC_PROFILES = {
    "vertex": "vs_6_0",
    "pixel": "ps_6_0",
    "compute": "cs_6_0",
    "geometry": "gs_6_0",
}

SLANG_STAGES = {
    "vertex": "vertex",
    "pixel": "fragment",
    "compute": "compute",
    "geometry": "geometry",
}

ENTRY_POINT = "main"

SCRIPT_DIR = Path(__file__).resolve().parent
# scripts/ -> makefiles/ -> repo root
ROOT_DIR = SCRIPT_DIR.parent.parent


def detect_stage(shader_name):
    lowered = shader_name.lower()
    for needles, stage in STAGE_RULES:
        if any(needle in lowered for needle in needles):
            return stage
    return None


def find_dxc():
    vulkan_sdk = os.getenv("VULKAN_SDK")
    if not vulkan_sdk:
        return None

    dxc = Path(vulkan_sdk) / "Bin" / "dxc.exe"
    return dxc if dxc.is_file() else None


def find_slangc():
    """Locate slangc, preferring the version pinned inside this repo.

    The vendored copy wins over anything on PATH or in the Vulkan SDK so that
    the language version is a property of the checkout, not of the machine.
    """
    vendored = ROOT_DIR / "external" / "slang" / "bin" / "slangc.exe"
    if vendored.is_file():
        return vendored

    override = os.getenv("SLANG_DIR")
    if override:
        candidate = Path(override) / "bin" / "slangc.exe"
        if candidate.is_file():
            return candidate
        candidate = Path(override) / "slangc.exe"
        if candidate.is_file():
            return candidate

    vulkan_sdk = os.getenv("VULKAN_SDK")
    if vulkan_sdk:
        # Vulkan SDK 1.3.296 and newer ship slangc alongside dxc.
        candidate = Path(vulkan_sdk) / "Bin" / "slangc.exe"
        if candidate.is_file():
            return candidate

    from shutil import which
    found = which("slangc")
    return Path(found) if found else None


def run(command, shader_path, target_file):
    try:
        subprocess.run(command, check=True)
    except subprocess.CalledProcessError as exception:
        print(f"Error: failed to compile {shader_path} (exit code {exception.returncode})")
        return False

    print(f"Compiled {shader_path} -> {target_file}")
    return True


def compile_hlsl(dxc, shader_path, target_file, include_dir, stage):
    command = [
        str(dxc),
        "-spirv",
        "-T", DXC_PROFILES[stage],
        "-E", ENTRY_POINT,
        "-I", str(include_dir),
        "-Fo", str(target_file),
        str(shader_path),
    ]
    return run(command, shader_path, target_file)


def compile_slang(slangc, shader_path, target_file, include_dir, stage):
    command = [
        str(slangc),
        str(shader_path),
        "-target", "spirv",
        "-stage", SLANG_STAGES[stage],
        "-entry", ENTRY_POINT,
        # Slang defaults to row-major, which emits as ColMajor in SPIR-V because
        # its terminology is inverted relative to SPIR-V. DXC's HLSL default
        # produces RowMajor. Without this flag the two languages would disagree
        # on matrix memory layout and every matrix in a UBO would read back
        # transposed, silently, with no diagnostic.
        "-matrix-layout-column-major",
        "-I", str(include_dir),
        "-o", str(target_file),
    ]
    return run(command, shader_path, target_file)


def process_shaders(source_dir, target_dir):
    source_path = Path(source_dir)
    target_path = Path(target_dir)

    if not source_path.is_dir():
        print(f"Error: source directory {source_dir} does not exist.")
        return 1

    target_path.mkdir(parents=True, exist_ok=True)

    shaders = sorted(
        list(source_path.glob("*.hlsl")) + list(source_path.glob("*.slang"))
    )

    # Resolved lazily so a project with no Slang shaders never needs slangc,
    # and vice versa.
    dxc = None
    slangc = None
    failed = []

    for shader_path in shaders:
        stage = detect_stage(shader_path.name)
        if stage is None:
            print(f"Warning: skipping {shader_path.name} (unknown shader type)")
            continue

        target_file = target_path / (shader_path.name + ".spv")

        if shader_path.suffix == ".hlsl":
            if dxc is None:
                dxc = find_dxc()
                if dxc is None:
                    print("Error: dxc.exe not found. Set VULKAN_SDK to your Vulkan SDK install.")
                    return 1
            ok = compile_hlsl(dxc, shader_path, target_file, source_path, stage)
        else:
            if slangc is None:
                slangc = find_slangc()
                if slangc is None:
                    print("Error: slangc.exe not found. Run 'py tools/download_slang.py'.")
                    return 1
            ok = compile_slang(slangc, shader_path, target_file, source_path, stage)

        if not ok:
            failed.append(shader_path.name)

    if failed:
        # Returning non-zero makes MSBuild surface the failure instead of
        # linking an executable against stale .spv files from a previous build.
        print(f"Error: {len(failed)} shader(s) failed to compile: {', '.join(failed)}")
        return 1

    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python compile_shaders_to_spirv.py <source_dir> <target_dir>")
        sys.exit(1)

    sys.exit(process_shaders(sys.argv[1], sys.argv[2]))
