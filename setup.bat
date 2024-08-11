@echo off

echo checking necessary python packages...
python tools/install_necessary_python_packages.py

echo generating glad for this project...
python tools/generate_submodules.py