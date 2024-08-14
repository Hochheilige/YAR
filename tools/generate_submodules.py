import subprocess
import time
import sys
import os
from tqdm import tqdm

def run_command_with_progress(command, estimated_time=30, description="Running command"):
    print(f"starting: {description}")

    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    with tqdm(total=100, desc=description, unit="%", ncols=100, ascii=True, file=sys.stdout) as progress_bar:
        start_time = time.time()
        while process.poll() is None:
            elapsed_time = time.time() - start_time
            progress = min((elapsed_time / estimated_time) * 100, 100)
            progress_bar.update(progress - progress_bar.n)
            time.sleep(0.001)

        process.communicate()
        progress_bar.update(100 - progress_bar.n)

    if process.returncode == 0:
        print(f"command completed successfully: {description}")
    else:
        print(f"command failed with return code {process.returncode}: {description}")
        print(process.stderr.read().decode())

def add_git_submodule(repo_url, submodule_path):
    if not os.path.isdir('.git'):
        raise RuntimeError("this directory is not a git repository")

    if os.path.isdir(submodule_path):
        print(f"path '{submodule_path}' are already exists")
        return

    print(f"adding submodule '{repo_url}' to '{submodule_path}'...")
    add_command = ['git', 'submodule', 'add', repo_url, submodule_path]
    update_command = ['git', 'submodule', 'update', '--init', '--recursive']

    estimated_time_add = 10
    estimated_time_update = 10

    run_command_with_progress(add_command, estimated_time=estimated_time_add, description="adding submodule")
    run_command_with_progress(update_command, estimated_time=estimated_time_update, description="updating submodule")

def build_project(project_path, project_name):
    configurations = ["Debug", "Release"]
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(tools_dir)
    build_dir = os.path.join(project_path, "build")
    cmake_command = [
        "cmake",
        "-S", project_path,
        "-B", build_dir, 
        f"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG={root_dir}/external/lib/Debug",
        f"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE={root_dir}/external/lib/Release",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DGLFW_LIBRARY_TYPE=STATIC",
        "-DGLFW_BUILD_EXAMPLES=0",
        "-DGLFW_BUILD_TESTS=0", 
        "-DGLFW_BUILD_DOCS=0"
    ]
    run_command_with_progress(cmake_command, estimated_time=15, description=f"Configuring {project_name} project")

    for config in configurations:
        # Create the build command for each configuration
        build_command = [
            "cmake", "--build", build_dir, "--config", config
        ]

        # Set the description for progress visualization
        description = f"Building {project_name} ({config} configuration)"

        # Run the build command with progress
        run_command_with_progress(build_command, estimated_time=15, description=description)

output_path = 'external/glad'
if os.path.isdir(output_path):
    print(f"path '{output_path}' are already exists")
else:
    command = [
        'python', '-m', 'glad', '--generator=c', '--spec=gl',
        '--out-path=' + output_path, '--api=gl=4.6', '--profile=core',
        '--extensions=GL_ARB_direct_state_access,GL_ARB_shader_draw_parameters,GL_ARB_shader_group_vote,GL_ARB_clip_control,GL_ARB_texture_filter_anisotropic,GL_ARB_texture_storage,GL_KHR_debug,GL_ARB_buffer_storage,GL_ARB_bindless_texture,GL_ARB_indirect_parameters,GL_ARB_gl_spirv'
    ]
    run_command_with_progress(command, estimated_time=2, description="generate glad")

submodules_urls = [
    "https://github.com/glfw/glfw.git",
    "https://github.com/nothings/stb.git",
    "https://github.com/ocornut/imgui.git"
]

submodules_paths = [
    "external/glfw" ,
    "external/stb",
    "external/imgui"
]

for url, path in zip(submodules_urls, submodules_paths):
    add_git_submodule(url, path)

build_project(submodules_paths[0], "glfw")