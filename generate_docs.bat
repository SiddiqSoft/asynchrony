@echo off
REM Generate Doxygen documentation for asynchrony library
REM This script generates the HTML documentation locally on Windows

setlocal enabledelayedexpansion

echo.
echo Asynchrony Library - Documentation Generator
echo ==================================================
echo.

REM Check if Doxygen is installed
where doxygen >nul 2>nul
if errorlevel 1 (
    echo Error: Doxygen is not installed
    echo.
    echo Please install Doxygen:
    echo   Download from https://www.doxygen.nl/download.html
    echo.
    echo Also install Graphviz:
    echo   Download from https://graphviz.org/download/
    exit /b 1
)

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0

REM Print Doxygen version
echo Doxygen version:
doxygen --version
echo.

REM Check if Doxyfile exists
if not exist "%SCRIPT_DIR%Doxyfile" (
    echo Error: Doxyfile not found in %SCRIPT_DIR%
    exit /b 1
)

REM Clean previous build
echo Cleaning previous documentation...
if exist "%SCRIPT_DIR%docs\doxygen" (
    rmdir /s /q "%SCRIPT_DIR%docs\doxygen"
)
echo Done
echo.

REM Generate documentation
echo Generating documentation...
cd /d "%SCRIPT_DIR%"
doxygen Doxyfile

REM Check if generation was successful
if exist "%SCRIPT_DIR%docs\doxygen\html" (
    echo.
    echo Documentation generated successfully!
    echo.
    echo Documentation location:
    echo   %SCRIPT_DIR%docs\doxygen\html\
    echo.
    echo Opening documentation in browser...
    start "" "%SCRIPT_DIR%docs\doxygen\html\index.html"
) else (
    echo.
    echo Error: Documentation generation failed
    exit /b 1
)

echo.
echo Done!
