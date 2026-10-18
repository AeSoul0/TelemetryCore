# NUCLEO-F446RE board support: HAL, CMSIS, FreeRTOS kernel and the firmware image.
#   STM32F446RE: Cortex-M4F @ 180 MHz, 512 KB flash, 128 KB SRAM

enable_language(ASM)

set(BSP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(HAL_DIR ${stm32f4xx_hal_SOURCE_DIR})
set(DEVICE_DIR ${cmsis_device_f4_SOURCE_DIR})
set(FREERTOS_DIR ${freertos_kernel_SOURCE_DIR})
set(LINKER_SCRIPT ${BSP_DIR}/linker/STM32F446RETx_FLASH.ld)

# --- CMSIS: Cortex-M4 core + STM32F446 device registers ------------------------------------------
add_library(cmsis_device INTERFACE)
target_include_directories(cmsis_device SYSTEM INTERFACE
    ${cmsis_core_SOURCE_DIR}/Include
    ${DEVICE_DIR}/Include)
target_compile_definitions(cmsis_device INTERFACE STM32F446xx)

# --- STM32 HAL (only the modules enabled in stm32f4xx_hal_conf.h are linked) ---------------------
set(HAL_MODULES hal hal_cortex hal_dma hal_exti hal_flash hal_flash_ex hal_gpio hal_pwr hal_pwr_ex hal_rcc
                hal_rcc_ex hal_tim hal_tim_ex hal_uart)
set(HAL_SOURCES)
foreach(module ${HAL_MODULES})
    list(APPEND HAL_SOURCES ${HAL_DIR}/Src/stm32f4xx_${module}.c)
endforeach()

add_library(stm32_hal STATIC ${HAL_SOURCES})
target_include_directories(stm32_hal SYSTEM PUBLIC ${HAL_DIR}/Inc)
target_include_directories(stm32_hal PUBLIC ${BSP_DIR}/include) # stm32f4xx_hal_conf.h
target_compile_definitions(stm32_hal PUBLIC USE_HAL_DRIVER)
target_link_libraries(stm32_hal PUBLIC cmsis_device)

# --- FreeRTOS kernel (static allocation only: no heap_x.c) --------------------------------------
add_library(freertos_kernel STATIC
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c)
target_include_directories(freertos_kernel SYSTEM PUBLIC
    ${FREERTOS_DIR}/include
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F)
target_include_directories(freertos_kernel PUBLIC ${BSP_DIR}/include) # FreeRTOSConfig.h

# --- Firmware image ------------------------------------------------------------------------------
add_executable(telemetry_fw
    src/main.cpp
    src/board.c
    src/stm32f4xx_it.c
    src/hal_msp.c
    src/hal_timebase_tim6.c
    src/syscalls.c
    ${DEVICE_DIR}/Source/Templates/system_stm32f4xx.c
    ${DEVICE_DIR}/Source/Templates/gcc/startup_stm32f446xx.s)
set_target_properties(telemetry_fw PROPERTIES SUFFIX ".elf" LINK_DEPENDS ${LINKER_SCRIPT})
target_include_directories(telemetry_fw PRIVATE include)
target_link_libraries(telemetry_fw PRIVATE app_core drivers_mock drivers_stm32 stm32_hal freertos_kernel telemetry_warnings)
target_link_options(telemetry_fw PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,-Map=$<TARGET_FILE_DIR:telemetry_fw>/telemetry_fw.map,--cref
    -Wl,--print-memory-usage)

add_custom_command(TARGET telemetry_fw POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:telemetry_fw> $<TARGET_FILE_DIR:telemetry_fw>/telemetry_fw.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:telemetry_fw> $<TARGET_FILE_DIR:telemetry_fw>/telemetry_fw.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:telemetry_fw>
    COMMENT "Generating telemetry_fw.hex / .bin")
