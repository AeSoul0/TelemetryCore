# Cross-compilation (ARM toolchain)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Set compilers
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-as)

# MCU Specific flags (e.g. for Cortex-M4)
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS "${MCU_FLAGS} -Os -Wall -fdata-sections -ffunction-sections" CACHE INTERNAL "C Compiler options")
set(CMAKE_CXX_FLAGS "${MCU_FLAGS} -Os -Wall -fdata-sections -ffunction-sections -fno-exceptions -fno-rtti" CACHE INTERNAL "C++ Compiler options")

set(CMAKE_EXE_LINKER_FLAGS "${MCU_FLAGS} -Wl,--gc-sections -Wl,-Map=${CMAKE_PROJECT_NAME}.map" CACHE INTERNAL "Linker options")

# Do not test the compiler during configuration (avoids missing syscalls error)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
