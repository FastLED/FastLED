@echo off
setlocal

rem Function to find Python executable
where python >nul 2>nul
if %errorlevel% equ 0 (
    set "PYTHON=python"
) else (
    where python3 >nul 2>nul
    if %errorlevel% equ 0 (
        set "PYTHON=python3"
    ) else (
        echo Python not found. Please install Python 3.
        exit /b 1
    )
)

rem Check if uv is installed, if not, install it
where uv >nul 2>nul
if %errorlevel% neq 0 (
    echo "uv" command not found. Please install "uv" by running "pip install uv" and try again.
    exit /b 1
)

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
