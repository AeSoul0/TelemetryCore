# Cross-compilation (ARM toolchain) for Cortex-M4F (STM32F4).
# Usage: cmake --preset stm32-debug   (or -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Set compilers (the assembler is driven through gcc so .s files go through the C preprocessor)
set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size CACHE FILEPATH "size")

# There is no OS to link a test executable against: check the compiler with a static library.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU Specific flags (Cortex-M4 with single-precision FPU, hard-float ABI)
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT "${MCU_FLAGS} -fdata-sections -ffunction-sections")
# Embedded C++ subset: no exceptions, no RTTI, no thread-safe static guards, no atexit destructors.
set(CMAKE_CXX_FLAGS_INIT
    "${MCU_FLAGS} -fdata-sections -ffunction-sections -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit")
set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS} -x assembler-with-cpp")

# newlib-nano + stub syscalls; drop unused sections.
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MCU_FLAGS} --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections")

foreach(lang C CXX ASM)
    set(CMAKE_${lang}_FLAGS_DEBUG_INIT "-Og -g3")
    set(CMAKE_${lang}_FLAGS_RELEASE_INIT "-O2 -g -DNDEBUG")
    set(CMAKE_${lang}_FLAGS_MINSIZEREL_INIT "-Os -g -DNDEBUG")
endforeach()

# Search headers and libraries only in the target environment, programs only on the host.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
