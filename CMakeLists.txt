cmake_minimum_required(VERSION 3.15)
project(bootloader VERSION 1.0)

# Specify the cross-compiler
set(CMAKE_SYSTEM_PROCESSOR arm)

# -------------------------------------------------------------------
# Toolchain
# -------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER /usr/bin/arm-none-eabi-gcc)
set(CMAKE_OBJCOPY /usr/bin/arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP /usr/bin/arm-none-eabi-objdump)
set(CMAKE_SIZE /usr/bin/arm-none-eabi-size)
set(CMAKE_AR /usr/bin/arm-none-eabi-ar)
set(CMAKE_RANLIB /usr/bin/arm-none-eabi-ranlib)
enable_language(ASM)

# -------------------------------------------------------------------
# Source directories
# -------------------------------------------------------------------
include_directories(
    ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${PROJECT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F0xx/Include
    ${PROJECT_SOURCE_DIR}/Drivers/STM32F0xx_HAL_Driver/Inc/Legacy
    ${PROJECT_SOURCE_DIR}/Drivers/STM32F0xx_HAL_Driver/Inc
    ${PROJECT_SOURCE_DIR}/Middleware/Inc
    ${PROJECT_SOURCE_DIR}/Core/Inc
)

# -------------------------------------------------------------------
# Linker script
# -------------------------------------------------------------------
set(LINKER_SCRIPT "${PROJECT_SOURCE_DIR}/STM32F030CCTX_FLASH.ld")

# -------------------------------------------------------------------
# Compiler and linker flags
# -------------------------------------------------------------------
set(COMMON_FLAGS "-mcpu=cortex-m0 -mthumb -ffreestanding -ffunction-sections -fdata-sections -Wall -nostdlib")

set(CMAKE_C_FLAGS "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "-T${LINKER_SCRIPT} -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs -lgcc")

# -------------------------------------------------------------------
# Sources
# -------------------------------------------------------------------
set(CORE_SOURCES
    Core/Src/system_stm32f0xx.c
    Core/Src/system.c
    Core/Src/sysmem.c
    Core/Src/syscalls.c
    Core/Src/bootloader.c
    Core/Src/main.c
    Core/Startup/startup_stm32f030cctx.S
)

file(GLOB_RECURSE DRIVERS_SOURCES
    ${PROJECT_SOURCE_DIR}/Drivers/STM32F0xx_HAL_Driver/Src/*.c
)

file(GLOB_RECURSE MIDDLEWARE_SOURCES
    ${PROJECT_SOURCE_DIR}/Middleware/Src/*.c
)

set(ALL_SOURCES
    ${CORE_SOURCES}
    ${DRIVERS_SOURCES}
    ${MIDDLEWARE_SOURCES}
)

# -------------------------------------------------------------------
# Executable
# -------------------------------------------------------------------
add_executable(bootloader ${ALL_SOURCES})

# Define preprocessor macros for the target
target_compile_definitions(bootloader PRIVATE STM32F030xC USE_HAL_DRIVER)

# Link ONLY the GCC support library (no libc, no startup)
target_link_libraries(bootloader PRIVATE -lgcc)

# -------------------------------------------------------------------
# Compiler options and definitions
# -------------------------------------------------------------------
target_compile_definitions(bootloader PRIVATE -DUSE_HAL_DRIVER)

# Optimization size
target_compile_options(bootloader PRIVATE -Os)

# -----------------------------
# Generate .bin after build
# -----------------------------
add_custom_command(TARGET bootloader POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bootloader> ${PROJECT_BINARY_DIR}/bootloader.bin
    COMMENT " --- Generating bootloader.bin"
)
