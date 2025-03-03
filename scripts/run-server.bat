@echo off
setlocal enabledelayedexpansion

:: XLauncher Server - Build and Run Script
:: Place this file in the scripts directory

echo =======================================================
echo XLauncher Server - Build and Run Script
echo =======================================================

:: Check if we're in the right directory
if not exist ..\CMakeLists.txt (
    echo Error: This script should be run from the scripts directory.
    echo Current directory is not within the project root.
    exit /b 1
)

:: Navigate to parent directory (project root)
cd ..

:: Parse command line arguments
set "command=%~1"
set "buildType=%~2"
if "!buildType!"=="" set "buildType=Release"

:: Show help if no command or help command
if "!command!"=="" goto :show_help
if /i "!command!"=="help" goto :show_help

:: Process commands
if /i "!command!"=="setup" goto :setup
if /i "!command!"=="build" goto :build
if /i "!command!"=="run" goto :run
if /i "!command!"=="clean" goto :clean
if /i "!command!"=="deps" goto :install_deps
if /i "!command!"=="all" goto :build_all

echo Unknown command: !command!
goto :show_help

:show_help
echo.
echo Usage: build-server.bat [command] [build-type]
echo.
echo Available commands:
echo   setup    - Clone submodules and setup vcpkg
echo   deps     - Install dependencies using vcpkg
echo   build    - Build the project
echo   run      - Run the server
echo   clean    - Clean build directory
echo   all      - Setup, install deps, and build
echo   help     - Show this help message
echo.
echo Available build types:
echo   Debug
echo   Release (default)
echo   RelWithDebInfo
echo   MinSizeRel
echo.
exit /b 0

:setup
echo.
echo === Setting up the project ===
echo.

echo Initializing and updating submodules...
git submodule update --init --recursive

echo.
echo Checking for vcpkg...
if not exist vcpkg (
    echo Cloning vcpkg...
    git clone https://github.com/Microsoft/vcpkg.git
    
    echo Bootstrapping vcpkg...
    cd vcpkg
    call bootstrap-vcpkg.bat
    cd ..
) else (
    echo vcpkg already installed.
)

echo.
echo Setup complete.
exit /b 0

:install_deps
echo.
echo === Installing dependencies ===
echo.

echo Installing required libraries...
vcpkg\vcpkg install nlohmann-json:x64-windows
vcpkg\vcpkg install openssl:x64-windows
vcpkg\vcpkg install zlib:x64-windows
vcpkg\vcpkg install ixwebsocket:x64-windows
vcpkg\vcpkg install libjpeg-turbo:x64-windows

echo.
echo Dependencies installed.
exit /b 0

:build
echo.
echo === Building the project (Build type: !buildType!) ===
echo.

if exist build (
    echo Removing existing build directory...
    rmdir /s /q build
)

echo Creating build directory...
mkdir build
cd build

echo Generating build files with CMake...
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=!buildType! -DCMAKE_CXX_FLAGS="-w" -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake ..

echo Building the project...
mingw32-make

cd ..
echo.
echo Build complete.
exit /b 0

:run
echo.
echo === Running the server ===
echo.

if not exist build\xlauncher-server.exe (
    echo Error: Server executable not found. Please build the project first.
    exit /b 1
)

echo Checking for config directory and files...
if not exist build\config (
    echo Creating config directory...
    mkdir build\config
)

if not exist build\config\apps.json (
    echo Creating default apps.json config file...
    echo { } > build\config\apps.json
)

echo Starting the server...
cd build
.\xlauncher-server.exe
cd ..

exit /b 0

:clean
echo.
echo === Cleaning build directory ===
echo.

if exist build (
    echo Removing build directory...
    rmdir /s /q build
    echo Build directory removed.
) else (
    echo Build directory not found.
)

exit /b 0

:build_all
echo.
echo === Running complete setup and build ===
echo.

call :setup
if %errorlevel% neq 0 (
    echo Setup failed.
    exit /b 1
)

call :install_deps
if %errorlevel% neq 0 (
    echo Dependencies installation failed.
    exit /b 1
)

call :build
if %errorlevel% neq 0 (
    echo Build failed.
    exit /b 1
)

echo.
echo All steps completed successfully.
echo You can now run the server with: build-server.bat run
echo.

exit /b 0