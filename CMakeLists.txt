cmake_minimum_required(VERSION 3.14)
project(Htkis-LANProxySerCli)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /W3)
endif()

include(FetchContent)

# Fetch asio
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG master
)
FetchContent_MakeAvailable(asio)

# Fetch glfw
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch imgui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(imgui)

# Find OpenGL
find_package(OpenGL REQUIRED)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Server executable (the only program)
add_executable(proxy_server
    src/server/Server.cpp
    src/server/GuiMain.cpp
    src/server/UserManager.cpp
    src/common/Localization.cpp
    ${IMGUI_SOURCES}
)

# Embed Windows resources (icon, etc.)
if(WIN32)
    target_sources(proxy_server PRIVATE src/server/resource.rc)
    # RC compiler needs to find HtkisPro.png relative to the .rc file
    set_source_files_properties(src/server/resource.rc PROPERTIES
        OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/HtkisPro.png
    )
endif()
target_include_directories(proxy_server PRIVATE
    src/server
    src/common
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${asio_SOURCE_DIR}/include
)
target_compile_definitions(proxy_server PRIVATE ASIO_STANDALONE)
if(MSVC)
    target_link_options(proxy_server PRIVATE /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS)
endif()
target_link_libraries(proxy_server PRIVATE glfw OpenGL32)
if(WIN32)
    target_link_libraries(proxy_server PRIVATE gdiplus)
endif()
