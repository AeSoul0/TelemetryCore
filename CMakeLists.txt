cmake_minimum_required(VERSION 3.21)
project(TelemetryCore
    VERSION 0.4.0
    DESCRIPTION "Embedded telemetry platform for STM32 (FreeRTOS) with a PC simulation"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TELEMETRY_WARNINGS_AS_ERRORS "Treat compiler warnings in project code as errors" OFF)
set(TELEMETRY_LOG_LEVEL 0 CACHE STRING "Lowest compiled-in log level: 0=DEBUG 1=INFO 2=WARN 3=ERROR 4=OFF")

if(CMAKE_CROSSCOMPILING)
    message(STATUS "TelemetryCore: firmware build for STM32 (${CMAKE_SYSTEM_PROCESSOR})")
else()
    message(STATUS "TelemetryCore: PC simulation build")
endif()

# Static Analysis and Quality (Compiler Warnings) - applied to project code only, never to
# third-party sources (HAL, CMSIS, FreeRTOS).
add_library(telemetry_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(telemetry_warnings INTERFACE
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wall -Wextra -Wpedantic -Wshadow -Wdouble-promotion -Wcast-align>
        $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor -Woverloaded-virtual>
        $<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<BOOL:${TELEMETRY_WARNINGS_AS_ERRORS}>>:-Werror>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(telemetry_warnings INTERFACE /W4 $<$<BOOL:${TELEMETRY_WARNINGS_AS_ERRORS}>:/WX>)
endif()

if(CMAKE_CROSSCOMPILING)
    include(cmake/stm32_dependencies.cmake)
endif()

# Layers, from the bottom up
add_subdirectory(platform)
add_subdirectory(drivers)
add_subdirectory(app)

if(CMAKE_CROSSCOMPILING)
    add_subdirectory(bsp/nucleo_f446re)
else()
    add_subdirectory(bsp/host)

    option(TELEMETRY_BUILD_TESTS "Build the unit tests" ON)
    if(TELEMETRY_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()
