@echo off
setlocal

rem if uv command is not found (using the where command) then warn the user and exit
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
