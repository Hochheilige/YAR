import subprocess
import threading
import time
import sys
import os
from tqdm import tqdm

import console_colors as color

def run_command_with_progress(command, estimated_time=30, description="Running command"):
    color.step(description)

    # stderr is merged into stdout so a single reader drains both: leaving either
    # pipe unread deadlocks the child once it fills the ~4-8KB pipe buffer.
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    output_lines = []

    def drain_output():
        for line in process.stdout:
            output_lines.append(line)

    reader = threading.Thread(target=drain_output, daemon=True)
    reader.start()

    # The bar is an elapsed-time estimate, not real progress: it is capped at 99%
    # while the command runs so that a full bar always means "actually finished".
    with tqdm(total=100, desc=description, unit="%", ncols=100, ascii=True, file=sys.stdout,
              colour=color.BAR_COLOR) as progress_bar:
        start_time = time.time()

        while process.poll() is None:
            elapsed_time = time.time() - start_time
            progress = min((elapsed_time / estimated_time) * 100, 99)
            progress_bar.update(progress - progress_bar.n)
            time.sleep(0.1)

        reader.join(timeout=5)
        progress_bar.update(100 - progress_bar.n)

    if process.returncode == 0:
        color.success(description)
    else:
        color.error(f"{description} (exit code {process.returncode})")
        color.detail("".join(output_lines[-20:]).strip())

    return process.returncode

def add_git_submodule(repo_url, submodule_path):
    if not os.path.isdir('.git'):
        raise RuntimeError("this directory is not a git repository")
    
    estimated_time_update = 60
    update_command = ['git', 'submodule', 'update', '--init', '--recursive', '--remote', '--', submodule_path]

    if os.path.isdir(submodule_path):
        color.detail(f"path '{submodule_path}' already exists, updating it")
        return run_command_with_progress(update_command, estimated_time=estimated_time_update, description=f"updating {submodule_path}")

    color.detail(f"adding submodule '{repo_url}' to '{submodule_path}'")

    estimated_time_add = 15
    add_command = ['git', 'submodule', 'add', repo_url, submodule_path]

    returncode = run_command_with_progress(add_command, estimated_time=estimated_time_add, description=f"adding {submodule_path}")
    if returncode != 0:
        return returncode

    return run_command_with_progress(update_command, estimated_time=estimated_time_update, description=f"updating {submodule_path}")

def build_project(project_path, project_name, cmake_additional_commands):
    configurations = ["Debug", "Release"]
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(tools_dir)
    build_dir = os.path.join(project_path, "build")
    cmake_command = [
        "cmake",
        "-S", project_path,
        "-B", build_dir, 
        "-DCMAKE_C_COMPILER=clang-cl",
        "-DCMAKE_CXX_COMPILER=clang-cl",
        f"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG={root_dir}/external/lib/Debug",
        f"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE={root_dir}/external/lib/Release",
    ]
    cmake_command += cmake_additional_commands
    if run_command_with_progress(cmake_command, estimated_time=15, description=f"Configuring {project_name} project") != 0:
        # Building against a failed configure only produces confusing follow-up errors.
        color.warn(f"skipping build of {project_name}: configure failed")
        return False

    for config in configurations:
        # Create the build command for each configuration
        build_command = [
            "cmake", "--build", build_dir, "--config", config
        ]

        # Set the description for progress visualization
        description = f"Building {project_name} ({config} configuration)"

        # Run the build command with progress
        if run_command_with_progress(build_command, estimated_time=120, description=description) != 0:
            return False

    return True

output_path = 'external/glad'
if os.path.isdir(output_path):
    color.detail(f"path '{output_path}' already exists, skipping glad generation")
else:
    command = [
        'python', '-m', 'glad', '--generator=c', '--spec=gl',
        '--out-path=' + output_path, '--api=gl=4.6', '--profile=core',
        '--extensions='
        'GL_ARB_explicit_uniform_location,'
        'GL_ARB_direct_state_access,'
        'GL_ARB_shader_draw_parameters,'
        'GL_ARB_shader_group_vote,'
        'GL_ARB_clip_control,'
        'GL_ARB_texture_filter_anisotropic,'
        'GL_ARB_texture_storage,'
        'GL_KHR_debug,'
        'GL_ARB_buffer_storage,'
        'GL_ARB_bindless_texture,'
        'GL_ARB_indirect_parameters,'
        'GL_ARB_gl_spirv,'
        'GL_ARB_shading_language_420pack,'
        # BC1/BC2/BC3 internal formats: S3TC is not core GL at any version.
        'GL_EXT_texture_compression_s3tc,'
        # sRGB variants of the S3TC formats live in a separate extension.
        'GL_EXT_texture_sRGB'
    ]
    run_command_with_progress(command, estimated_time=2, description="generate glad")

submodules = {
    "external/stb": "https://github.com/nothings/stb.git",
    "external/imgui": "https://github.com/ocornut/imgui.git",
    "external/spirv-reflect": "https://github.com/KhronosGroup/SPIRV-Reflect.git",
    "external/spirv-cross": "https://github.com/KhronosGroup/SPIRV-Cross.git",
    "external/assimp": "https://github.com/assimp/assimp",
    "external/meshoptimizer": "https://github.com/zeux/meshoptimizer.git",
    "external/directx-math": "https://github.com/microsoft/DirectXMath.git",
    "external/directx-tex": "https://github.com/microsoft/DirectXTex.git",
}

failed = []

for path, url in submodules.items():
    if add_git_submodule(url, path) != 0:
        failed.append(path)

if not build_project("external/assimp", "assimp",
        ["-DBUILD_SHARED_LIBS=OFF",
         "-DASSIMP_BUILD_TESTS=OFF",
         "-DASSIMP_INSTALL=ON",
         "-DASSIMP_INJECT_DEBUG_POSTFIX=ON",
         "-DASSIMP_BUILD_ASSIMP_VIEW=OFF"
        ]
):
    failed.append("assimp")

if not build_project("external/meshoptimizer", "meshoptimizer",
        ["-DMESHOPT_BUILD_DEMO=OFF",
         "-DMESHOPT_BUILD_GLTFPACK=OFF",
         "-DMESHOPT_BUILD_SHARED_LIBS=OFF",
         "-DMESHOPT_WERROR=OFF",
         "-DMESHOPT_INSTALL=ON"
        ]
):
    failed.append("meshoptimizer")

if not build_project("external/directx-tex", "directx-tex",
        ["-DBUILD_SAMPLE=OFF"
        ]
):
    failed.append("directx-tex")

print()
if failed:
    color.error(f"setup finished with errors in: {', '.join(failed)}")
    sys.exit(1)

color.success("setup finished successfully")
