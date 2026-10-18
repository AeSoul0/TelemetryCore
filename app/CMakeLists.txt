# Application layer: identical sources for the PC simulation and the STM32 firmware.
# It only depends on interfaces (OSAL, drivers/include), never on HAL or <thread>.

add_library(app_core STATIC
    src/acquisition.cpp
    src/telemetry.cpp
    src/uplink.cpp
    src/tasks.cpp
)
target_include_directories(app_core PUBLIC include)
target_link_libraries(app_core
    PUBLIC platform_core osal drivers_interfaces
    PRIVATE telemetry_warnings)
target_compile_definitions(app_core PRIVATE TELEMETRY_FW_VERSION="${PROJECT_VERSION}")
