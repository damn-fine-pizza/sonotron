# Cross toolchain for the STM32 anchor part (D33: STM32H743, Cortex-M7).
# Minimum arm-none-eabi-gcc version: 14 (C++26 subset; see docs/DESIGN.md §20).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# Link test binaries as static libs so try_compile works without a linker script.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(ARRANGRR_ARM_FLAGS
    "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS_INIT "${ARRANGRR_ARM_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${ARRANGRR_ARM_FLAGS}")

# nosys: newlib syscall stubs so a hosted-looking main() links in the stub image.
set(CMAKE_EXE_LINKER_FLAGS_INIT "--specs=nosys.specs -Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
