import subprocess
import sys

def install_package(package_name):
    try:
        # Check that package installed
        __import__(package_name)
        print(f"{package_name} is already installed.")
    except ImportError:
        print(f"{package_name} not found. Installing...")
        # Install package using pip
        subprocess.check_call([sys.executable, "-m", "pip", "install", package_name])
        print(f"{package_name} has been installed.")


# Necessary packages:
# -- tqdm - to visualize instalation
# -- glad - to generate glad files for project
packages = ["tqdm", "glad"]

for package in packages:
    install_package(package)