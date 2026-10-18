# PC "board": simulation entry point with mock drivers and stdout as serial link.

add_executable(telemetry_sim main_host.cpp)
target_link_libraries(telemetry_sim PRIVATE app_core drivers_mock drivers_host telemetry_warnings)
