@echo off

setlocal

set DEPOT_TOOLS_WIN_TOOLCHAIN=0
set PATH=%PATH%;%~dp0

python "%~dp0\build.py" %*