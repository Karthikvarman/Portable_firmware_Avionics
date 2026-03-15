@echo off
REM AeroFirmware Real-time Build Script (Windows)
REM This script builds the firmware and ensures all binary files are generated

setlocal enabledelayedexpansion

echo ==========================================
echo AeroFirmware Real-time Build Script
echo ==========================================

REM Default MCU target
set MCU_TARGET=%1
if "%MCU_TARGET%"=="" set MCU_TARGET=STM32H7

set BUILD_TYPE=%2
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo Building for: %MCU_TARGET%
echo Build type: %BUILD_TYPE%
echo ==========================================

REM Clean previous build
echo Cleaning previous build...
if exist build rmdir /s /q build
mkdir build

REM Configure build
echo Configuring CMake...
cmake -B build -DTARGET_MCU=%MCU_TARGET% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    exit /b 1
)

REM Build firmware
echo Building firmware...
cmake --build build --config %BUILD_TYPE% --parallel
if errorlevel 1 (
    echo ERROR: Build failed!
    exit /b 1
)

REM Verify binary files
echo ==========================================
echo Verifying generated binary files...
echo ==========================================

cd build

REM Check for ELF file
if exist "AeroFirmware.elf" (
    echo ✅ ELF file generated: AeroFirmware.elf
    for %%A in (AeroFirmware.elf) do echo    Size: %%~zA bytes
) else (
    echo ❌ ELF file NOT found!
    exit /b 1
)

REM Check for BIN file
if exist "AeroFirmware.bin" (
    echo ✅ BIN file generated: AeroFirmware.bin
    for %%A in (AeroFirmware.bin) do echo    Size: %%~zA bytes
) else (
    echo ❌ BIN file NOT found!
    exit /b 1
)

REM Check for HEX file (STM32 only)
echo %MCU_TARGET% | findstr /i "STM32" >nul
if errorlevel 1 (
    REM Not STM32
) else (
    if exist "AeroFirmware.hex" (
        echo ✅ HEX file generated: AeroFirmware.hex
        for %%A in (AeroFirmware.hex) do echo    Size: %%~zA bytes
    ) else (
        echo ❌ HEX file NOT found!
        exit /b 1
    )
)

REM Check for disassembly (STM32 only)
echo %MCU_TARGET% | findstr /i "STM32" >nul
if errorlevel 1 (
    REM Not STM32
) else (
    if exist "AeroFirmware.dis" (
        echo ✅ Disassembly generated: AeroFirmware.dis
    ) else (
        echo ⚠️  Disassembly file NOT found (optional)
    )
)

echo ==========================================
echo Build Summary:
echo ==========================================
echo ✅ All required binary files generated successfully!
echo ✅ Firmware includes:
echo    - NEO-F10 GPS support
echo    - WiFi connectivity (native or external)
echo    - Bluetooth connectivity (native or external)
echo    - Real-time telemetry
echo    - Wireless communication manager
echo    - RTOS task coordination
echo.
echo Files ready for flashing:
echo    - ELF: AeroFirmware.elf (debugging)
echo    - BIN: AeroFirmware.bin (flashing)
echo %MCU_TARGET% | findstr /i "STM32" >nul
if errorlevel 1 (
    REM Not STM32
) else (
    echo    - HEX: AeroFirmware.hex (flashing)
)
echo.
echo Flash commands:
echo %MCU_TARGET% | findstr /i "STM32" >nul
if errorlevel 1 (
    REM Not STM32
    echo    ESP: esptool.py --chip %MCU_TARGET% write_flash 0x0 AeroFirmware.bin
) else (
    echo    STM32: STM32_Programmer_CLI -c port=SWD -w AeroFirmware.hex -v -rst
)

echo ==========================================
echo Build completed successfully! 🚀
echo ==========================================

cd ..

exit /b 0
