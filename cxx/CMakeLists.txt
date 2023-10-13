# Minimum version is CMake 3.26
cmake_minimum_required(VERSION 3.26)

# Export compile commands for the language server
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Project instantiation
project(axolotlsd VERSION 0.4.0.1)

# Configure the project header
configure_file(include/configuration.txt
    ${PROJECT_SOURCE_DIR}/include/configuration.hpp)

# Build our main library
add_library(${PROJECT_NAME}_s STATIC src/axolotlsd.cpp)
add_library(${PROJECT_NAME} SHARED src/axolotlsd.cpp)

# Use C++20 on target too
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED TRUE)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME}_s PROPERTY CXX_STANDARD_REQUIRED TRUE)
set_property(TARGET ${PROJECT_NAME}_s PROPERTY CXX_STANDARD 20)

# Set target library properties
set_target_properties(${PROJECT_NAME}
    PROPERTIES
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
    SOVERSION 0
    PUBLIC_HEADER include/axolotlsd.hpp)
set_target_properties(${PROJECT_NAME}_s
    PROPERTIES
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
    PUBLIC_HEADER include/axolotlsd.hpp)

# Finally link
target_link_libraries(${PROJECT_NAME})

# Allow installation
install(
    TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_s
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
