@echo off
setlocal

rem Change to the directory of the batch file
cd /d "%~dp0"

rem Check if .venv directory exists
if not exist .venv (
    rem Create virtual environment
    call install.bat
)

set "interactive_stmt="
rem Check if no arguments were provided
if "%~1"=="" (
    set "interactive_stmt=--interactive"
)

rem Run the Python script
.venv\Scripts\python.exe ci\ci-compile.py %interactive_stmt% %*
