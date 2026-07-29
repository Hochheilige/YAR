@echo off

python tools/install_necessary_python_packages.py
if errorlevel 1 exit /b 1

python tools/generate_submodules.py
if errorlevel 1 exit /b 1
