@echo off

setlocal

set DEPOT_TOOLS_WIN_TOOLCHAIN=0
set PATH=%PATH%;%~dp0

python3 "%~dp0\build.py" %*