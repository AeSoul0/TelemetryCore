# Unit and integration tests, run on the PC with the host OSAL backend.

include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    test_acquisition.cpp
    test_bit_utils.cpp
    test_frame.cpp
    test_logger.cpp
    test_mock_drivers.cpp
    test_osal.cpp
    test_pipeline.cpp
    test_telemetry.cpp
)
target_link_libraries(unit_tests PRIVATE app_core drivers_mock GTest::gtest_main telemetry_warnings)

include(GoogleTest)
gtest_discover_tests(unit_tests DISCOVERY_TIMEOUT 30)
