cmake_minimum_required(VERSION 3.20)
project(EmbeddedTelemetry CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Phase 19: Static Analysis and Quality (Compiler Warnings)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
endif()

# Define include directories
include_directories(
    app/include
    drivers/include
    platform/include
)

# Collect source files
set(APP_SOURCES
    app/src/main.cpp
    app/src/tasks.cpp
    app/src/telemetry.cpp
)

set(DRIVER_SOURCES
    drivers/src/sensor.cpp
)

# Create the executable
add_executable(embedded_telemetry
    ${APP_SOURCES}
    ${DRIVER_SOURCES}
)

# Optional Phase 18 setup for multi-config (Debug/Release)
# Usually managed via `cmake --build . --config Release`
