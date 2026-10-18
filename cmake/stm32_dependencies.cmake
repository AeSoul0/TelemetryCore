# Third-party firmware sources, downloaded at configure time and pinned to exact tags.
# SOURCE_SUBDIR points to a directory that does not exist, so FetchContent only downloads the
# sources: the libraries are defined by bsp/nucleo_f446re/CMakeLists.txt with our own flags.

include(FetchContent)

FetchContent_Declare(freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG V11.1.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _sources_only)

FetchContent_Declare(cmsis_core
    GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis-core.git
    GIT_TAG v5.9.0_20250520
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _sources_only)

FetchContent_Declare(cmsis_device_f4
    GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis-device-f4.git
    GIT_TAG v2.6.11
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _sources_only)

FetchContent_Declare(stm32f4xx_hal
    GIT_REPOSITORY https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git
    GIT_TAG v1.8.5
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _sources_only)

FetchContent_MakeAvailable(freertos_kernel cmsis_core cmsis_device_f4 stm32f4xx_hal)
