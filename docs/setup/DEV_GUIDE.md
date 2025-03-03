# Developer Guide for XLauncher Server

This guide provides instructions for setting up, building, and running the XLauncher Server application.

## Command Usage

### For Windows PowerShell

```powershell
cd scripts
.\build-server.bat setup              # Clone submodules and setup vcpkg
.\build-server.bat deps               # Install dependencies using vcpkg
.\build-server.bat build [build-type] # Build the project
.\build-server.bat run                # Run the server
.\build-server.bat clean              # Clean build directory
.\build-server.bat all                # Setup, install deps, and build
.\build-server.bat help               # Show help information
```

### For Git Bash

```bash
cd scripts
./build-server.bat setup              # Clone submodules and setup vcpkg
./build-server.bat deps               # Install dependencies using vcpkg
./build-server.bat build [build-type] # Build the project
./build-server.bat run                # Run the server
./build-server.bat clean              # Clean build directory
./build-server.bat all                # Setup, install deps, and build
./build-server.bat help               # Show help information
```