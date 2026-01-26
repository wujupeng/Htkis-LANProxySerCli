<<<<<<< HEAD
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
    target_link_libraries(lanproxy-server ws2_32 mswsock)
else()
    target_link_libraries(lanproxy-server pthread)
endif()

# Client
add_executable(lanproxy-client src/client/main.cpp)
if(WIN32)
    target_link_libraries(lanproxy-client ws2_32 mswsock)
else()
    target_link_libraries(lanproxy-client pthread)
endif()
=======
cmake_minimum_required(VERSION 3.14)
project(Htkis-LANProxySerCli)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable threading support
find_package(Threads REQUIRED)

if(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    find_library(OPENGL_LIBRARY OpenGL)
endif()

# Fetch Dependencies
include(FetchContent)

# 1. Asio
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-28-0
)
FetchContent_MakeAvailable(asio)

# 2. GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# 3. Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.9
)
FetchContent_MakeAvailable(imgui)

# Common includes
include_directories(${asio_SOURCE_DIR}/asio/include)
include_directories(src/common)

# Setup ImGui Source Files
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Server GUI Executable
add_executable(proxy_server_gui
    src/server/GuiMain.cpp
    src/server/Server.cpp
    src/server/UserManager.cpp
    src/common/Localization.cpp
    ${IMGUI_SOURCES}
)
target_include_directories(proxy_server_gui PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${glfw_SOURCE_DIR}/include
)
target_link_libraries(proxy_server_gui 
    Threads::Threads 
    glfw
)

# Client GUI Executable
add_executable(proxy_client_gui
    src/client/GuiMain.cpp
    src/client/Client.cpp
    src/common/Localization.cpp
    ${IMGUI_SOURCES}
)
target_include_directories(proxy_client_gui PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${glfw_SOURCE_DIR}/include
)
target_link_libraries(proxy_client_gui 
    Threads::Threads 
    glfw
)

if(APPLE)
    target_link_libraries(proxy_server_gui ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY} ${OPENGL_LIBRARY})
    target_link_libraries(proxy_client_gui ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY} ${OPENGL_LIBRARY})
elseif(WIN32)
    target_link_libraries(proxy_server_gui ws2_32 opengl32)
    target_link_libraries(proxy_client_gui ws2_32 opengl32)
else()
    target_link_libraries(proxy_server_gui GL)
    target_link_libraries(proxy_client_gui GL)
endif()

# Keep CLI versions for reference/testing if needed (optional)
# add_executable(proxy_server_cli src/server/main.cpp src/server/Server.cpp src/server/UserManager.cpp)
# target_link_libraries(proxy_server_cli Threads::Threads)
# ...
>>>>>>> 8d260dd7167b147a3ab95360cd19a3183772b52a
