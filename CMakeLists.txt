# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.18)

# Define the project
project(LynxEngine VERSION 0.1.0)

# Set the C++ standard for the whole project
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/MP)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(FetchContent)

FetchContent_Declare(
    nvrhi
    GIT_REPOSITORY https://github.com/NVIDIA-RTX/NVRHI.git
    GIT_TAG 06d4fdc82e2d911e510210c3b9ec0c68830ccda4
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.5-docking
)

FetchContent_Declare(
    msdf-atlas-gen
    GIT_REPOSITORY https://github.com/Chlumsky/msdf-atlas-gen.git
    GIT_TAG v1.3
)

FetchContent_MakeAvailable(nvrhi)
FetchContent_MakeAvailable(imgui)

set(MSDF_ATLAS_GEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_GEN_USE_VCPKG ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(msdf-atlas-gen)

# This enables vcpkg's manifest mode. It will automatically download
# and build the libraries listed in vcpkg.json.
# The CMAKE_TOOLCHAIN_FILE variable must be set when you configure cmake.
set(VCPKG_ROOT "${CMAKE_SOURCE_DIR}/vcpkg" CACHE PATH "vcpkg root directory")
if (DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}" CACHE PATH "vcpkg root directory" FORCE)
endif()
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain file")

# Add the subdirectories. CMake will process the CMakeLists.txt in each.
# The order matters as it defines dependencies.
add_subdirectory(engine)
add_subdirectory(editor)
add_subdirectory(game)
add_subdirectory(game_dll)
add_subdirectory(game_standalone)