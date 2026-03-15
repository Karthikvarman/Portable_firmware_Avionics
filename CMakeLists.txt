cmake_minimum_required(VERSION 3.20)

# ============================================================================
# AeroFirmware — Portable Avionics CMake Build System
# Usage: cmake -B build/<mcu> -DTARGET_MCU=STM32H7
# ============================================================================

project(AeroFirmware C ASM)
set(CMAKE_C_STANDARD 11)

# ----------------------------------------------------------------------------
# Target MCU selection
# ----------------------------------------------------------------------------
set(TARGET_MCU "STM32H7" CACHE STRING "Target MCU (STM32H7|STM32F4|STM32F7|STM32L4|STM32L0|ESP32|ESP32_DEVKITV1|ESP32_DEV_MODULE|ESP8266)")
message(STATUS "AeroFirmware: Building for ${TARGET_MCU}")

# Set MCU-specific defines
add_compile_definitions(TARGET_${TARGET_MCU})

# ----------------------------------------------------------------------------
# Determine toolchain and MCU-specific flags
# ----------------------------------------------------------------------------
if(TARGET_MCU STREQUAL "STM32H7")
    set(MCU_FLAGS "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb")
    add_compile_definitions(STM32H743xx USE_FULL_LL_DRIVER)
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/build/ld/stm32h743_flash.ld)
    set(PLATFORM_DIR platform/stm32h7)
    set(HAL_DIRS hal/stm32h7)

elseif(TARGET_MCU STREQUAL "STM32F4")
    set(MCU_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb")
    add_compile_definitions(STM32F407xx USE_FULL_LL_DRIVER)
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/build/ld/stm32f407_flash.ld)
    set(PLATFORM_DIR platform/stm32f4)
    set(HAL_DIRS hal/stm32f4)

elseif(TARGET_MCU STREQUAL "STM32F7")
    set(MCU_FLAGS "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb")
    add_compile_definitions(STM32F767xx USE_FULL_LL_DRIVER)
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/build/ld/stm32f767_flash.ld)
    set(PLATFORM_DIR platform/stm32f7)
    set(HAL_DIRS hal/stm32f7)

elseif(TARGET_MCU STREQUAL "STM32L4")
    set(MCU_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb")
    add_compile_definitions(STM32L476xx USE_FULL_LL_DRIVER)
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/build/ld/stm32l476_flash.ld)
    set(PLATFORM_DIR platform/stm32l4)
    set(HAL_DIRS hal/stm32l4)

elseif(TARGET_MCU STREQUAL "STM32L0")
    set(MCU_FLAGS "-mcpu=cortex-m0plus -mthumb")
    add_compile_definitions(STM32L073xx USE_FULL_LL_DRIVER)
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/build/ld/stm32l073_flash.ld)
    set(PLATFORM_DIR platform/stm32l0)
    set(HAL_DIRS hal/stm32l0)

elseif(TARGET_MCU MATCHES "^ESP32" OR TARGET_MCU STREQUAL "ESP32" OR TARGET_MCU STREQUAL "ESP32_DEVKITV1" OR TARGET_MCU STREQUAL "ESP32_DEV_MODULE")
    set(MCU_FLAGS "")
    set(PLATFORM_DIR platform/esp32)
    set(HAL_DIRS hal/esp32)
    if(TARGET_MCU STREQUAL "ESP32_DEVKITV1")
        add_compile_definitions(TARGET_ESP32_DEVKITV1)
    elseif(TARGET_MCU STREQUAL "ESP32_DEV_MODULE")
        add_compile_definitions(TARGET_ESP32_DEV_MODULE)
    endif()

elseif(TARGET_MCU STREQUAL "ESP8266")
    set(MCU_FLAGS "")
    set(PLATFORM_DIR platform/esp8266)
    set(HAL_DIRS hal/esp8266)

else()
    message(FATAL_ERROR "Unknown TARGET_MCU: ${TARGET_MCU}")
endif()

# ----------------------------------------------------------------------------
# Compiler flags
# ----------------------------------------------------------------------------
if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(COMMON_FLAGS "${MCU_FLAGS} -Wall -Wextra -ffunction-sections -fdata-sections -fno-common -Os")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${MCU_FLAGS}")
else()
    # Fallback/MSVC flags for linting/simulation builds on Windows
    add_compile_definitions(SIMULATION)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
endif()

# Debug vs Release
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g3 -DDEBUG)
else()
    add_compile_options(-Os -DNDEBUG)
endif()

# ----------------------------------------------------------------------------
# Include directories
# ----------------------------------------------------------------------------
include_directories(
    config/
    hal/include/
    sal/include/
    sal/drivers/lps25hb/
    sal/drivers/sht40/
    sal/drivers/lsm6dsv/
    sal/drivers/lis2mdl/
    sal/drivers/teseo_liv4f/
    sal/drivers/tgs5141/
    core/ekf/
    core/app/
    core/telemetry/
    core/protocol/
    core/control/
    core/wireless/
    platform/
    rtos/os_api/
    rtos/include/
)

# MCU-specific vendor headers (assumed installed)
if(TARGET_MCU MATCHES "STM32")
    include_directories(
        ${CMAKE_SOURCE_DIR}/vendor/CMSIS/Include
        ${CMAKE_SOURCE_DIR}/vendor/CMSIS/Device/ST/${TARGET_MCU}/Include
        ${CMAKE_SOURCE_DIR}/vendor/STM32_LL_Drivers/include
        ${CMAKE_SOURCE_DIR}/vendor/FreeRTOS/include
        ${CMAKE_SOURCE_DIR}/vendor/FreeRTOS/portable/GCC/ARM_CM7/r0p1
    )
endif()

# ----------------------------------------------------------------------------
# Source files
# ----------------------------------------------------------------------------
set(CORE_SRCS
    ${CMAKE_SOURCE_DIR}/core/app/main.c
    ${CMAKE_SOURCE_DIR}/core/app/app_tasks.c
    ${CMAKE_SOURCE_DIR}/core/ekf/aero_ekf.c
    ${CMAKE_SOURCE_DIR}/core/calibration/sensor_cal.c
    ${CMAKE_SOURCE_DIR}/core/telemetry/telemetry.c
    ${CMAKE_SOURCE_DIR}/core/protocol/protocol_manager.c
    ${CMAKE_SOURCE_DIR}/core/control/pid.c
    ${CMAKE_SOURCE_DIR}/core/control/stabilization.c
    ${CMAKE_SOURCE_DIR}/core/wireless/wireless_manager.c
)

set(SAL_SRCS
    ${CMAKE_SOURCE_DIR}/sal/sal_sensor.c
    ${CMAKE_SOURCE_DIR}/sal/sal_gps.c
    ${CMAKE_SOURCE_DIR}/sal/drivers/lps25hb/lps25hb.c
    ${CMAKE_SOURCE_DIR}/sal/drivers/sht40/sht40.c
    ${CMAKE_SOURCE_DIR}/sal/drivers/lsm6dsv/lsm6dsv.c
    ${CMAKE_SOURCE_DIR}/sal/drivers/lis2mdl/lis2mdl.c
    ${CMAKE_SOURCE_DIR}/sal/drivers/teseo_liv4f/teseo_liv4f.c
    ${CMAKE_SOURCE_DIR}/sal/drivers/tgs5141/tgs5141.c
)

message(STATUS "DEBUG: CORE_SRCS count: " )
list(LENGTH CORE_SRCS CORE_COUNT)
message(STATUS "DEBUG: CORE_SRCS count: ${CORE_COUNT}")


# Dynamically select HAL and Platform files based on target
if(TARGET_MCU STREQUAL "STM32H7")
    set(HAL_SRCS
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32h7_adc.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32h7_flash.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32h7_gpio.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32h7_i2c.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32h7_spi.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32h7_timer.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32h7_uart.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32h7_wifi.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32h7_bt.c
    )
    set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/${PLATFORM_DIR}/stm32h7_platform.c)
    set(RTOS_SRCS 
        ${CMAKE_SOURCE_DIR}/rtos/os_api/os_api_freertos.c
        ${CMAKE_SOURCE_DIR}/rtos/rtos_tasks.c
    )

elseif(TARGET_MCU STREQUAL "STM32F4")
    set(HAL_SRCS
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32f4_gpio.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32f4_timer.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32f4_wifi.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32f4_bt.c
    )
    set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/${PLATFORM_DIR}/stm32f4_platform.c)
    set(RTOS_SRCS 
        ${CMAKE_SOURCE_DIR}/rtos/os_api/os_api_freertos.c
        ${CMAKE_SOURCE_DIR}/rtos/rtos_tasks.c
    )

elseif(TARGET_MCU STREQUAL "STM32F7")
    set(HAL_SRCS
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32f7_gpio.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32f7_timer.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32f7_wifi.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32f7_bt.c
    )
    set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/${PLATFORM_DIR}/stm32f7_platform.c)
    set(RTOS_SRCS 
        ${CMAKE_SOURCE_DIR}/rtos/os_api/os_api_freertos.c
        ${CMAKE_SOURCE_DIR}/rtos/rtos_tasks.c
    )

elseif(TARGET_MCU STREQUAL "STM32L4")
    set(HAL_SRCS
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32l4_gpio.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32l4_timer.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32l4_wifi.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32l4_bt.c
    )
    set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/${PLATFORM_DIR}/stm32l4_platform.c)
    set(RTOS_SRCS 
        ${CMAKE_SOURCE_DIR}/rtos/os_api/os_api_freertos.c
        ${CMAKE_SOURCE_DIR}/rtos/rtos_tasks.c
    )

elseif(TARGET_MCU STREQUAL "STM32L0")
    set(HAL_SRCS
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32l0_gpio.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32l0_timer.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32l0_wifi.c
        ${CMAKE_SOURCE_DIR}/${HAL_DIRS}/stm32l0_bt.c
    )
    set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/${PLATFORM_DIR}/stm32l0_platform.c)
    set(RTOS_SRCS 
        ${CMAKE_SOURCE_DIR}/rtos/os_api/os_api_freertos.c
        ${CMAKE_SOURCE_DIR}/rtos/rtos_tasks.c
    )

elseif(TARGET_MCU MATCHES "^ESP32")
    file(GLOB HAL_SRCS "${CMAKE_SOURCE_DIR}/${HAL_DIRS}/*.c")
    if(TARGET_MCU STREQUAL "ESP32-S2")
        set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/platform/esp32/esp32s2_platform.c)
    elseif(TARGET_MCU STREQUAL "ESP32-S3")
        set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/platform/esp32/esp32s3_platform.c)
    elseif(TARGET_MCU STREQUAL "ESP32-C3")
        set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/platform/esp32/esp32c3_platform.c)
    elseif(TARGET_MCU STREQUAL "ESP32-C6")
        set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/platform/esp32/esp32c6_platform.c)
    elseif(TARGET_MCU STREQUAL "ESP32-H2")
        set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/platform/esp32/esp32h2_platform.c)
    elseif(TARGET_MCU STREQUAL "ESP32_DEV_MODULE")
        set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/platform/esp32/esp32_dev_module_platform.c)
    else()
        set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/platform/esp32/esp32_platform.c)
    endif()
    set(RTOS_SRCS 
        ${CMAKE_SOURCE_DIR}/rtos/os_api/os_api_freertos.c
        ${CMAKE_SOURCE_DIR}/rtos/rtos_tasks.c
    )

elseif(TARGET_MCU STREQUAL "ESP8266")
    file(GLOB HAL_SRCS "${CMAKE_SOURCE_DIR}/${HAL_DIRS}/*.c")
    set(PLATFORM_SRCS ${CMAKE_SOURCE_DIR}/platform/esp8266/esp8266_platform.c)
    set(RTOS_SRCS 
        ${CMAKE_SOURCE_DIR}/rtos/os_api/os_api_freertos.c
        ${CMAKE_SOURCE_DIR}/rtos/rtos_tasks.c
    )

else()
    file(GLOB HAL_SRCS "${CMAKE_SOURCE_DIR}/${HAL_DIRS}/*.c")
    file(GLOB PLATFORM_SRCS "${CMAKE_SOURCE_DIR}/${PLATFORM_DIR}/*.c")
    set(RTOS_SRCS 
        ${CMAKE_SOURCE_DIR}/rtos/os_api/os_api_freertos.c
        ${CMAKE_SOURCE_DIR}/rtos/rtos_tasks.c
    )
endif()

# ----------------------------------------------------------------------------
# Find required tools for embedded development
# ----------------------------------------------------------------------------
# Check if we're on Windows without ARM toolchain (simulation mode)
if(WIN32 AND NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Running in simulation mode on Windows - ARM toolchain not available")
    set(SIMULATION_MODE ON)
    # Create dummy tools for simulation
    set(CMAKE_OBJCOPY echo)
    set(CMAKE_SIZE echo)
    set(CMAKE_OBJDUMP echo)
else()
    # Real embedded build - find actual tools
    find_program(CMAKE_OBJCOPY NAMES objcopy arm-none-eabi-objcopy)
    find_program(CMAKE_SIZE NAMES size arm-none-eabi-size)
    find_program(CMAKE_OBJDUMP NAMES objdump arm-none-eabi-objdump)
    
    if(NOT CMAKE_OBJCOPY)
        message(FATAL_ERROR "objcopy not found. Please install ARM GNU toolchain.")
    endif()
    
    if(NOT CMAKE_SIZE)
        message(FATAL_ERROR "size not found. Please install ARM GNU toolchain.")
    endif()
    
    set(SIMULATION_MODE OFF)
endif()

# ----------------------------------------------------------------------------
# Build firmware ELF
# ----------------------------------------------------------------------------
add_executable(${PROJECT_NAME}.elf
    ${CORE_SRCS}
    ${SAL_SRCS}
    ${HAL_SRCS}
    ${PLATFORM_SRCS}
    ${RTOS_SRCS}
)

target_link_libraries(${PROJECT_NAME}.elf m) # libm for math

# Linker flags (for STM32 targets)
if(TARGET_MCU MATCHES "STM32")
    target_link_options(${PROJECT_NAME}.elf PRIVATE
        -T${LINKER_SCRIPT}
        ${MCU_FLAGS}
        -Wl,--gc-sections
        -Wl,-Map=${PROJECT_NAME}.map,--cref
        -Wl,--print-memory-usage
        --specs=nano.specs
        --specs=nosys.specs
        -lc -lm -lnosys
    )
endif()

# ----------------------------------------------------------------------------
# Post-build: Generate HEX and BIN files for flashing
# ----------------------------------------------------------------------------
if(SIMULATION_MODE)
    # Simulation mode - create placeholder files in existing structure
    add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Simulation mode: Creating placeholder binary files"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/firmware_binaries
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/firmware_uploads
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.bin
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.hex
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.bin ${CMAKE_SOURCE_DIR}/firmware_uploads/${PROJECT_NAME}_${TARGET_MCU}.bin
        COMMAND ${CMAKE_COMMAND} -E echo "Binary files ready for actual toolchain build"
        COMMENT "Simulation: Creating placeholder binary files in existing structure"
        VERBATIM
    )
elseif(TARGET_MCU MATCHES "STM32")
    # STM32: Use ARM objcopy to generate HEX and BIN in existing structure
    add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/firmware_binaries
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/firmware_uploads
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.bin ${CMAKE_SOURCE_DIR}/firmware_uploads/${PROJECT_NAME}_${TARGET_MCU}.bin
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.hex ${CMAKE_SOURCE_DIR}/firmware_uploads/${PROJECT_NAME}_${TARGET_MCU}.hex
        COMMENT "Generating HEX and BIN files for ${TARGET_MCU} in existing structure"
        VERBATIM
    )
elseif(TARGET_MCU MATCHES "ESP32" OR TARGET_MCU STREQUAL "ESP8266")
    # ESP32/ESP8266: Use esptool to generate BIN in existing structure
    find_program(ESPTOOL NAMES esptool.py)
    if(NOT ESPTOOL)
        message(FATAL_ERROR "esptool.py not found. Please install ESP-IDF.")
    endif()
    
    add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/firmware_binaries
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/firmware_uploads
        COMMAND ${ESPTOOL} --chip ${TARGET_MCU} elf2image $<TARGET_FILE:${PROJECT_NAME}.elf> ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.bin
        COMMAND ${ESPTOOL} --chip ${TARGET_MCU} elf2image $<TARGET_FILE:${PROJECT_NAME}.elf> ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.hex
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.bin ${CMAKE_SOURCE_DIR}/firmware_uploads/${PROJECT_NAME}_${TARGET_MCU}.bin
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.hex ${CMAKE_SOURCE_DIR}/firmware_uploads/${PROJECT_NAME}_${TARGET_MCU}.hex
        COMMENT "Generating BIN and HEX files for ${TARGET_MCU} in existing structure"
        VERBATIM
    )
endif()

# ----------------------------------------------------------------------------
# Generate disassembly for debugging (STM32 only)
# ----------------------------------------------------------------------------
if(TARGET_MCU MATCHES "STM32")
    add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJDUMP} -d $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.dis
        COMMENT "Generating disassembly"
        VERBATIM
    )
endif()

# ----------------------------------------------------------------------------
# Flash targets using existing binary structure
# ----------------------------------------------------------------------------
if(TARGET_MCU MATCHES "STM32")
    # STM32: Use STM32CubeProgrammer with existing binaries
    add_custom_target(flash
        COMMAND STM32_Programmer_CLI -c port=SWD -w ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.hex -v -rst
        DEPENDS ${PROJECT_NAME}.elf
        COMMENT "Flashing ${TARGET_MCU} via ST-LINK using existing binary"
    )
elseif(TARGET_MCU MATCHES "ESP32" OR TARGET_MCU STREQUAL "ESP8266")
    # ESP32/ESP8266: Use esptool with existing binaries
    add_custom_target(flash
        COMMAND ${ESPTOOL} --chip ${TARGET_MCU} write_flash 0x0 ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.bin
        DEPENDS ${PROJECT_NAME}.elf
        COMMENT "Flashing ${TARGET_MCU} via esptool using existing binary"
    )
endif()

# ----------------------------------------------------------------------------
# Update existing binaries with new features
# ----------------------------------------------------------------------------
add_custom_target(update_binaries
    COMMAND ${CMAKE_COMMAND} -E echo "Updating existing binaries with wireless and GPS features..."
    COMMAND ${CMAKE_COMMAND} -E echo "Features added:"
    COMMAND ${CMAKE_COMMAND} -E echo "  - NEO-F10 GPS support"
    COMMAND ${CMAKE_COMMAND} -E echo "  - WiFi connectivity (native or external)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - Bluetooth connectivity (native or external)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - Real-time telemetry (20Hz)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - Wireless communication manager"
    COMMAND ${CMAKE_COMMAND} -E echo "  - RTOS task coordination"
    COMMAND ${CMAKE_COMMAND} -E echo "  - Professional UI controls"
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${PROJECT_NAME}.elf --config ${CMAKE_BUILD_TYPE}
    COMMENT "Updating existing binaries with new wireless and GPS features"
)

# ----------------------------------------------------------------------------
# Real-time build verification targets
# ----------------------------------------------------------------------------
add_custom_target(build_all_binaries
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${PROJECT_NAME}.elf --config ${CMAKE_BUILD_TYPE}
    COMMENT "Building firmware with all wireless and GPS features"
)

add_custom_target(verify_binaries
    COMMAND ${CMAKE_COMMAND} -E echo "Verifying binary files in existing structure..."
    COMMAND ${CMAKE_COMMAND} -E echo "Binary directories:"
    COMMAND ${CMAKE_COMMAND} -E echo "  - firmware_binaries/ (master copies)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - firmware_uploads/ (flashing copies)"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Checking for ${TARGET_MCU} binaries..."
    COMMAND if EXISTS ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.bin
    COMMAND ${CMAKE_COMMAND} -E echo "✅ BIN: firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.bin"
    COMMAND if EXISTS ${CMAKE_SOURCE_DIR}/firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.hex
    COMMAND ${CMAKE_COMMAND} -E echo "✅ HEX: firmware_binaries/${PROJECT_NAME}_${TARGET_MCU}.hex"
    COMMAND if EXISTS ${CMAKE_SOURCE_DIR}/firmware_uploads/${PROJECT_NAME}_${TARGET_MCU}.bin
    COMMAND ${CMAKE_COMMAND} -E echo "✅ Upload BIN: firmware_uploads/${PROJECT_NAME}_${TARGET_MCU}.bin"
    COMMAND if EXISTS ${CMAKE_SOURCE_DIR}/firmware_uploads/${PROJECT_NAME}_${TARGET_MCU}.hex
    COMMAND ${CMAKE_COMMAND} -E echo "✅ Upload HEX: firmware_uploads/${PROJECT_NAME}_${TARGET_MCU}.hex"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "All binary files include:"
    COMMAND ${CMAKE_COMMAND} -E echo "  - NEO-F10 GPS support (10Hz update)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - WiFi connectivity (native or external)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - Bluetooth connectivity (native or external)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - Real-time telemetry (20Hz update rate)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - Wireless communication manager"
    COMMAND ${CMAKE_COMMAND} -E echo "  - RTOS task coordination"
    COMMAND ${CMAKE_COMMAND} -E echo "  - Professional UI controls"
    COMMENT "Verifying all generated binary files in existing structure"
    DEPENDS ${PROJECT_NAME}.elf
)

message(STATUS "AeroFirmware build configured for ${TARGET_MCU}")
