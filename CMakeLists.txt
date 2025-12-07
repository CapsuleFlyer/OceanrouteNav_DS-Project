cmake_minimum_required(VERSION 3.19)
project(OceanrouteNav LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: prefer release builds. We rely on vcpkg CMake toolchain file.
# Call cmake with -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

find_package(SFML 3 COMPONENTS System Window Graphics Audio Network CONFIG REQUIRED)

add_executable(oceanroute main.cpp)
target_link_libraries(oceanroute PRIVATE SFML::System SFML::Window SFML::Graphics SFML::Audio SFML::Network)

if (WIN32)
	# Automatically copy SFML DLLs from vcpkg bin to the build output directory so the exe can run
	# Adjust VCPKG_BIN_DIR if you installed vcpkg to a different location
	set(VCPKG_BIN_DIR "C:/vcpkg/installed/x64-windows/bin")
	file(GLOB SFML_DLLS "${VCPKG_BIN_DIR}/sfml-*.dll")
	foreach(dll ${SFML_DLLS})
		add_custom_command(TARGET oceanroute POST_BUILD
						   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${dll} $<TARGET_FILE_DIR:oceanroute>)
	endforeach()
endif()