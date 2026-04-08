cmake_minimum_required(VERSION 3.13)

# Auto-enable vcpkg in local environments when VCPKG_ROOT is defined.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(codecrafters-claude-code)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard

find_package(CURL CONFIG REQUIRED)
find_package(cJSON CONFIG REQUIRED)

add_executable(claude-code ${SOURCE_FILES})
target_link_libraries(claude-code PRIVATE CURL::libcurl cjson)
