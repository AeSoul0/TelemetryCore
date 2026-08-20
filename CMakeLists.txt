cmake_minimum_required(VERSION 3.20)
project(EmbeddedTelemetry CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(embedded_telemetry
    src/main.cpp
    src/sensor.cpp
    src/telemetry.cpp
)
