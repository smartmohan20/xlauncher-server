# XLauncher Server - Installation and Setup Guide

## Prerequisites

### Software Requirements
- Git
- CMake (3.12 or higher)
- MinGW-w64 Compiler
- Visual Studio Build Tools (optional, but recommended)

### Recommended Tools
- Visual Studio Code
- Postman (for WebSocket testing)

## Installation Steps

### 1. Clone the Repository

```bash
# Clone the main repository
git clone https://github.com/yourusername/xlauncher-server.git
cd xlauncher-server

# Clone IXWebSocket submodule
git submodule add https://github.com/machinezone/IXWebSocket.git lib/ixwebsocket

# Update submodules
git submodule update --init --recursive
```

### 2. Install Vcpkg (Package Manager)

```bash
# Clone Vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap Vcpkg
.\bootstrap-vcpkg.bat  # For Windows
./bootstrap-vcpkg.bat  # For Git Bash

# Return to project root
cd ..
```

### 3. Install Dependencies

```bash
# Install required libraries
./vcpkg/vcpkg install nlohmann-json:x64-windows
./vcpkg/vcpkg install openssl:x64-windows
./vcpkg/vcpkg install zlib:x64-windows
./vcpkg/vcpkg install ixwebsocket:x64-windows
./vcpkg/vcpkg install libjpeg-turbo
```

### 4. Configure Project

```bash
# Remove build directory if it exists
rm -rf build

# Create build directory
mkdir build
cd build

# Generate CMake files
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-w" -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake ..

```

### 5. Build the Project

```bash
# Build the project
mingw32-make
```

### 6. Running the Server

```bash
# Run the server
.\xlauncher-server.exe  # For Windows
./xlauncher-server      # For Git Bash
```

## WebSocket Testing

### Postman Configuration
1. Open Postman
2. Go to WebSocket tab
3. URL: `ws://localhost:2354`

### Test Scenarios

#### List Applications
```json
{
  "type": "list_apps"
}
```

#### Launch Chrome
```json
{
  "type": "launch_app",
  "data": {
    "path": "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
    "arguments": ["--profile-last-used", "google.com"]
  }
}
```

## Troubleshooting

### Common Issues
- Ensure all dependencies are installed
- Check firewall settings
- Verify application paths
- Confirm MinGW and CMake are in system PATH

### Dependency Installation Alternatives
- Use Chocolatey on Windows
- Use MSYS2 for MinGW packages

## Advanced Configuration

Add the following environment variables to your `.env` file:
```env
# Application Configuration
APP_ENV="YOUR_APP_ENV"                           # Set your app environment (development, staging, or production)
APP_NAME="YOUR_APP_NAME"                         # Your application name

# Server Core Settings
PORT="YOUR_PORT_NUMBER"                          # Port number for your application
HOST="YOUR_HOST_ADDRESS"                         # Host address to bind the server
MAX_CONNECTIONS="YOUR_MAX_CONNECTIONS"           # Maximum number of concurrent connections
LOG_LEVEL="YOUR_LOG_LEVEL"                       # Logging level (DEBUG, INFO, WARNING, ERROR)

# WebSocket Configuration
WEBSOCKET_TIMEOUT="YOUR_WEBSOCKET_TIMEOUT"       # WebSocket connection timeout in seconds
WEBSOCKET_MAX_PAYLOAD="YOUR_MAX_PAYLOAD_SIZE"    # Maximum WebSocket payload size in bytes
WEBSOCKET_PING_INTERVAL="YOUR_PING_INTERVAL"     # WebSocket ping interval in seconds

# Application Management
APP_CONFIG_PATH="YOUR_CONFIG_PATH"               # Path to application configuration file
MAX_REGISTERED_APPS="YOUR_MAX_APPS"              # Maximum number of registered applications
DEFAULT_APP_LAUNCH_TIMEOUT="YOUR_LAUNCH_TIMEOUT" # Default timeout for application launch in seconds

# Security Configuration
CORS_ALLOWED_ORIGINS="YOUR_ALLOWED_ORIGINS"      # Comma-separated list of allowed CORS origins
ENABLE_SSL="YOUR_SSL_SETTING"                    # Enable/disable SSL (true/false)
SSL_CERT_PATH="YOUR_SSL_CERT_PATH"               # Path to SSL certificate
SSL_KEY_PATH="YOUR_SSL_KEY_PATH"                 # Path to SSL key

# Logging Configuration
LOG_FILE_PATH="YOUR_LOG_FILE_PATH"               # Path to log file
LOG_ROTATION_SIZE="YOUR_LOG_ROTATION_SIZE"       # Log file rotation size
LOG_BACKUP_COUNT="YOUR_BACKUP_COUNT"             # Number of log backups to keep

# Performance Tuning
THREAD_POOL_SIZE="YOUR_THREAD_POOL_SIZE"         # Size of the thread pool
SOCKET_RECEIVE_BUFFER_SIZE="YOUR_RECEIVE_BUFFER" # Socket receive buffer size in bytes
SOCKET_SEND_BUFFER_SIZE="YOUR_SEND_BUFFER"       # Socket send buffer size in bytes

# Debug Mode
DEBUG_MODE="YOUR_DEBUG_SETTING"                  # Enable/disable debug mode (true/false)
VERBOSE_LOGGING="YOUR_VERBOSE_SETTING"           # Enable/disable verbose logging (true/false)
```
