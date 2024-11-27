import os
import sys
import subprocess
from pathlib import Path

def compile_shader(shader_path, target_path):
    shader_name = shader_path.name
    if "vert" in shader_name:
        target_profile = "vs_6_0"
    elif "frag" in shader_name:
        target_profile = "ps_6_0"
    else:
        print(f"Warning: Skipping {shader_name} (unknown shader type)")
        return
    
    target_file = target_path / (shader_name + ".spv")

    # It will work only if Vulkan installed and Vulkan in PATH
    # so probably better use DXC as submodule and compile it myself
    vulkan_sdk_path = os.getenv("VULKAN_SDK")
    dxc = vulkan_sdk_path + "\Bin\dxc.exe"

    try:
        subprocess.run(
            [
                dxc,
                "-spirv",
                "-T", target_profile,        
                "-E", "main",   
                "-I", ".",             
                "-Fo", str(target_file),     
                str(shader_path)             
            ],
            check=True,
        )
        print(f"Compiled {shader_path} -> {target_file}")
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to compile {shader_path}. {e}")

def process_shaders(source_dir, target_dir):
    source_path = Path(source_dir)
    target_path = Path(target_dir)
    
    if not source_path.is_dir():
        print(f"Error: Source directory {source_dir} does not exist.")
        sys.exit(1)
    
    target_path.mkdir(parents=True, exist_ok=True)

    for shader_path in source_path.glob("*.hlsl"):
        compile_shader(shader_path, target_path)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python compile_shaders.py <source_dir> <target_dir>")
        sys.exit(1)
    
    source_dir = sys.argv[1]
    target_dir = sys.argv[2]
    process_shaders(source_dir, target_dir)
