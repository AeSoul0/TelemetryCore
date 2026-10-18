# Platform layer: error codes, bit utilities, framing, logger and the OS abstraction layer.

add_library(platform_core STATIC
    src/frame.cpp
    src/logger.cpp
)
target_include_directories(platform_core PUBLIC include)
target_compile_definitions(platform_core PUBLIC TELEMETRY_LOG_LEVEL=${TELEMETRY_LOG_LEVEL})
target_link_libraries(platform_core PRIVATE telemetry_warnings)

# OSAL: same API (include/osal/osal.h), backend chosen by the build target.
if(CMAKE_CROSSCOMPILING)
    add_library(osal STATIC osal_freertos/osal_freertos.cpp)
    target_include_directories(osal PUBLIC include osal_freertos/include)
    target_link_libraries(osal PUBLIC freertos_kernel PRIVATE telemetry_warnings)
else()
    find_package(Threads REQUIRED)
    add_library(osal STATIC osal_host/osal_host.cpp)
    target_include_directories(osal PUBLIC include osal_host/include)
    target_link_libraries(osal PUBLIC Threads::Threads PRIVATE telemetry_warnings)
endif()
