# Driver layer: abstract interfaces + one implementation set per platform.

add_library(drivers_interfaces INTERFACE)
target_include_directories(drivers_interfaces INTERFACE include)
target_link_libraries(drivers_interfaces INTERFACE platform_core)

# Mock drivers are portable: they run in the PC simulation and on the STM32 (milestone M5).
add_library(drivers_mock STATIC
    mock/src/mock_env_sensor.cpp
    mock/src/mock_battery_gauge.cpp
)
target_include_directories(drivers_mock PUBLIC mock/include)
target_link_libraries(drivers_mock PUBLIC drivers_interfaces PRIVATE telemetry_warnings)

if(CMAKE_CROSSCOMPILING)
    add_library(drivers_stm32 STATIC stm32/src/stm32_uart.cpp)
    target_include_directories(drivers_stm32 PUBLIC stm32/include)
    target_link_libraries(drivers_stm32 PUBLIC drivers_interfaces stm32_hal PRIVATE telemetry_warnings)
else()
    add_library(drivers_host STATIC host/src/stdout_serial.cpp)
    target_include_directories(drivers_host PUBLIC host/include)
    target_link_libraries(drivers_host PUBLIC drivers_interfaces PRIVATE telemetry_warnings)
endif()
