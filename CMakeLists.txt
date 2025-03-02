# Vcpkg Toolchain Configuration
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
elseif(DEFINED CMAKE_TOOLCHAIN_FILE)
    # Do nothing, toolchain file is already set
else()
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

cmake_minimum_required(VERSION 3.12)
project(xlauncher-server)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Try to find TurboJPEG but make it optional
find_package(TurboJPEG QUIET)

if(TURBOJPEG_FOUND)
    add_definitions(-DHAVE_TURBOJPEG)
    include_directories(${TURBOJPEG_INCLUDE_DIRS})
    set(TURBOJPEG_LIBS ${TURBOJPEG_LIBRARIES})
    message(STATUS "TurboJPEG found, enabling high-performance screen sharing")
else()
    set(TURBOJPEG_LIBS "")
    add_definitions(-DNO_TURBOJPEG)
    message(STATUS "TurboJPEG not found, screen sharing will use GDI+ fallback method")
endif()

# Add IXWebSocket library
add_subdirectory(lib/ixwebsocket)

# Include directories
include_directories(
    include
    src
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/ixwebsocket
)

# Create executable
add_executable(xlauncher-server
    src/main.cpp
    src/server/server.cpp
    src/server/websocket_server.cpp
    src/application/app_launcher.cpp
    src/screen_capture/screen_capture.cpp
    src/input/input_handler.cpp
    src/screen_sharing.cpp
    src/utils/base64.cpp
)

# Link libraries
target_link_libraries(xlauncher-server
    PRIVATE ixwebsocket
    PRIVATE OpenSSL::SSL
    PRIVATE OpenSSL::Crypto
    PRIVATE Shell32      # For ShellExecute
    PRIVATE ws2_32
    PRIVATE crypt32
    PRIVATE Comctl32     # For icon extraction functions
    PRIVATE Gdi32        # For GDI functions
    PRIVATE gdiplus      # For GDI+ (fallback for JPEG compression)
    ${ZLIB_LIBRARIES}
    ${TURBOJPEG_LIBS}
)

# Copy .env file to build directory
configure_file(${CMAKE_SOURCE_DIR}/.env ${CMAKE_BINARY_DIR}/.env COPYONLY)