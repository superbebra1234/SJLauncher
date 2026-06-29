cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

project(SJLauncher)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)

include_directories(src)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(SJLauncher ${SOURCES})

target_link_libraries(SJLauncher PRIVATE
    glfw
    imgui::imgui
    OpenGL::GL
    cpr::cpr
    nlohmann_json::nlohmann_json
    libzip::zip
)
