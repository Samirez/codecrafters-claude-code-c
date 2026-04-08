cmake_minimum_required(VERSION 3.13)

# Auto-enable vcpkg in local environments when VCPKG_ROOT is defined.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
	if(EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
		set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
	endif()
endif()

project(codecrafters-claude-code)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard

# Try to find packages, with fallback to MSYS64 path
find_package(CURL CONFIG QUIET)
if(NOT CURL_FOUND)
	# Set up MSYS64 paths as fallback
	if(NOT DEFINED OPENSSL_ROOT_DIR AND EXISTS "C:/msys64/ucrt64")
		set(OPENSSL_ROOT_DIR "C:/msys64/ucrt64" CACHE STRING "")
		set(ZLIB_ROOT "C:/msys64/ucrt64" CACHE STRING "")
	endif()
	find_package(CURL REQUIRED)
endif()

# Try to find cJSON, but make it optional
find_package(cJSON CONFIG QUIET)
if(NOT cJSON_FOUND)
	# Try alternative names for cJSON
	find_package(cjson CONFIG QUIET)
endif()
if(NOT cJSON_FOUND AND NOT cjson_FOUND)
	# If cJSON is not found via config, try module mode
	find_package(cJSON MODULE QUIET)
endif()

# If still not found, build it from third_party
if(NOT cJSON_FOUND AND NOT cjson_FOUND)
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cJSON/CMakeLists.txt")
		add_subdirectory("third_party/cJSON" cJSON)
		set(cJSON_FOUND TRUE)
	endif()
endif()

add_executable(claude-code ${SOURCE_FILES})

# Build link libraries list
set(LINK_LIBS CURL::libcurl)
if(cJSON_FOUND OR cjson_FOUND)
	if(TARGET cjson)
		list(APPEND LINK_LIBS cjson)
	else()
		list(APPEND LINK_LIBS cJSON)
	endif()
endif()

target_link_libraries(claude-code PRIVATE ${LINK_LIBS})

# Add include paths for cJSON
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cJSON")
	target_include_directories(claude-code PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cJSON")
endif()
