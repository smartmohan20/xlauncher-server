# Developer Guide for XLauncher Server

This guide provides instructions for setting up, building, and running the XLauncher Server application.

## Command Usage

### For Windows PowerShell

```powershell
cd scripts
.\run-server.bat setup              # Clone submodules and setup vcpkg
.\run-server.bat deps               # Install dependencies using vcpkg
.\run-server.bat build [build-type] # Build the project
.\run-server.bat run                # Run the server
.\run-server.bat clean              # Clean build directory
.\run-server.bat all                # Setup, install deps, and build
.\run-server.bat help               # Show help information
```

### For Git Bash

```bash
cd scripts
./run-server.bat setup              # Clone submodules and setup vcpkg
./run-server.bat deps               # Install dependencies using vcpkg
./run-server.bat build [build-type] # Build the project
./run-server.bat run                # Run the server
./run-server.bat clean              # Clean build directory
./run-server.bat all                # Setup, install deps, and build
./run-server.bat help               # Show help information
```