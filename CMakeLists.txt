# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.18)

# Define the project
project(LynxEngine VERSION 0.1.0)

# Set the C++ standard for the whole project
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(FetchContent)

FetchContent_Declare(
    nvrhi
    GIT_REPOSITORY https://github.com/NVIDIA-RTX/NVRHI.git
    GIT_TAG 06d4fdc82e2d911e510210c3b9ec0c68830ccda4
)

FetchContent_MakeAvailable(nvrhi)

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