cmake_minimum_required(VERSION 3.10)
project(Htkis-LANProxySerCli VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check for Asio
find_path(ASIO_INCLUDE_DIR asio.hpp HINTS "${CMAKE_SOURCE_DIR}/third_party/asio/include")

if(NOT ASIO_INCLUDE_DIR)
    message(WARNING "Asio not found! Please download standalone asio to third_party/asio/ or install it.")
    message(STATUS "You can download it from: https://think-async.com/Asio/Download.html")
    # We will assume it might be in include path if not found specifically
else()
    include_directories(${ASIO_INCLUDE_DIR})
    message(STATUS "Found Asio at: ${ASIO_INCLUDE_DIR}")
endif()

# Common includes
include_directories(src/common)

# Server
add_executable(lanproxy-server src/server/main.cpp)
if(WIN32)
    target_link_libraries(lanproxy-server ws2_32)
else()
    target_link_libraries(lanproxy-server pthread)
endif()

# Client
add_executable(lanproxy-client src/client/main.cpp)
if(WIN32)
    target_link_libraries(lanproxy-client ws2_32)
else()
    target_link_libraries(lanproxy-client pthread)
endif()
