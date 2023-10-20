# =============================================================================
#   Copyright 2023 Roland Metivier <metivier.roland@chlorophyt.us>
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# =============================================================================
#   AxolotlSD for C++ CMakeFile, generates a test program with Raylib
# Minimum version is CMake 3.26
cmake_minimum_required(VERSION 3.26)

# Export compile commands for the language server
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Raylib config to OpenGL 4.3
set(GRAPHICS GRAPHICS_API_OPENGL_43)

# Project instantiation
project(axolotlsd_test VERSION 0.1.0.2)

# Find AxolotlSD
include(FindPkgConfig)
pkg_check_modules(AxolotlSD REQUIRED axolotlsd)

# Add raylib
add_subdirectory(raylib)

# Configure the project header
configure_file(include/configuration.txt
    ${PROJECT_SOURCE_DIR}/include/configuration.hpp)

# Build our main executable
add_executable(${PROJECT_NAME}
    src/axolotlsd_test.cpp)

# Use C++20 on target too
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED TRUE)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

# Include headers here
target_include_directories(${PROJECT_NAME} PRIVATE include raylib/src)

# Finally link
target_link_libraries(${PROJECT_NAME} raylib ${AxolotlSD_LIBRARIES})
