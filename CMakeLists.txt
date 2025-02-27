cmake_minimum_required(VERSION 3.12)
project(xlauncher-server)

set(CMAKE_CXX_STANDARD 17)

# Find ZLIB
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

# Add IXWebSocket library
add_subdirectory(lib/ixwebsocket)

# Add nlohmann/json library
add_subdirectory(lib/json)

# Create include directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/include)

# Copy our custom dotenv implementation to the include directory
configure_file(
    ${CMAKE_SOURCE_DIR}/src/dotenv.h
    ${CMAKE_SOURCE_DIR}/include/dotenv.h
    COPYONLY
)

include_directories(
    include
    lib/ixwebsocket
    ${CMAKE_SOURCE_DIR}/src
)

add_executable(xlauncher-server
    src/main.cpp
    src/server/server.cpp
    src/server/websocket_server.cpp
)

target_link_libraries(xlauncher-server
    PRIVATE ixwebsocket
    PRIVATE nlohmann_json::nlohmann_json
    Shell32
    ws2_32
    crypt32
    ${ZLIB_LIBRARIES}
)

# Copy .env file to build directory
configure_file(${CMAKE_SOURCE_DIR}/.env ${CMAKE_BINARY_DIR}/.env COPYONLY)