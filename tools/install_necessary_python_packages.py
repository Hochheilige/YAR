import subprocess
import sys

import console_colors as color

def install_package(package_name):
    try:
        # Check that package installed
        __import__(package_name)
        color.detail(f"{package_name} is already installed")
    except ImportError:
        color.step(f"{package_name} not found, installing")
        # Install package using pip
        try:
            subprocess.check_call([sys.executable, "-m", "pip", "install", package_name])
        except subprocess.CalledProcessError as exception:
            color.error(f"failed to install {package_name} (exit code {exception.returncode})")
            return False
        color.success(f"{package_name} has been installed")

    return True


# Necessary packages:
# -- tqdm - to visualize instalation
# -- glad - to generate glad files for project
packages = ["tqdm", "glad"]

if not all([install_package(package) for package in packages]):
    sys.exit(1)
