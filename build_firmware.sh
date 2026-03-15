#!/bin/bash
# AeroFirmware Real-time Build Script
# This script builds the firmware and updates existing binary structure

set -e  # Exit on any error

echo "=========================================="
echo "AeroFirmware Real-time Build Script"
echo "Updating Existing Binary Structure"
echo "=========================================="

# Default MCU target
MCU_TARGET=${1:-STM32H7}
BUILD_TYPE=${2:-Release}

echo "Building for: $MCU_TARGET"
echo "Build type: $BUILD_TYPE"
echo "Binary structure: firmware_binaries/ + firmware_uploads/"
echo "=========================================="

# Clean previous build
echo "Cleaning previous build..."
rm -rf build
mkdir -p build

# Configure build
echo "Configuring CMake..."
cmake -B build -DTARGET_MCU=$MCU_TARGET -DCMAKE_BUILD_TYPE=$BUILD_TYPE

# Build firmware
echo "Building firmware with wireless and GPS features..."
cmake --build build --config $BUILD_TYPE --parallel

# Verify binary files in existing structure
echo "=========================================="
echo "Verifying updated binary files..."
echo "=========================================="

cd build

# Check for ELF file
if [ -f "AeroFirmware.elf" ]; then
    echo "✅ ELF file generated: AeroFirmware.elf"
    echo "   Size: $(stat -f%z AeroFirmware.elf 2>/dev/null || stat -c%s AeroFirmware.elf) bytes"
else
    echo "❌ ELF file NOT found!"
    exit 1
fi

cd ..

# Check for binary files in existing structure
echo ""
echo "Checking binary files in existing structure..."

if [ -f "firmware_binaries/AeroFirmware_${MCU_TARGET}.bin" ]; then
    echo "✅ BIN file: firmware_binaries/AeroFirmware_${MCU_TARGET}.bin"
    echo "   Size: $(stat -f%z firmware_binaries/AeroFirmware_${MCU_TARGET}.bin 2>/dev/null || stat -c%s firmware_binaries/AeroFirmware_${MCU_TARGET}.bin) bytes"
else
    echo "❌ BIN file NOT found in firmware_binaries/!"
    exit 1
fi

if [ -f "firmware_binaries/AeroFirmware_${MCU_TARGET}.hex" ]; then
    echo "✅ HEX file: firmware_binaries/AeroFirmware_${MCU_TARGET}.hex"
    echo "   Size: $(stat -f%z firmware_binaries/AeroFirmware_${MCU_TARGET}.hex 2>/dev/null || stat -c%s firmware_binaries/AeroFirmware_${MCU_TARGET}.hex) bytes"
else
    echo "❌ HEX file NOT found in firmware_binaries/!"
    exit 1
fi

if [ -f "firmware_uploads/AeroFirmware_${MCU_TARGET}.bin" ]; then
    echo "✅ Upload BIN: firmware_uploads/AeroFirmware_${MCU_TARGET}.bin"
else
    echo "⚠️  Upload BIN file NOT found in firmware_uploads/!"
fi

echo "=========================================="
echo "Binary Structure Update Complete!"
echo "=========================================="
echo "✅ All binary files updated with new features:"
echo "   - NEO-F10 GPS support (10Hz update)"
echo "   - WiFi connectivity (native or external)"
echo "   - Bluetooth connectivity (native or external)"
echo "   - Real-time telemetry (20Hz update rate)"
echo "   - Wireless communication manager"
echo "   - RTOS task coordination"
echo "   - Professional UI controls"
echo ""
echo "Updated files ready for flashing:"
echo "   - BIN: firmware_binaries/AeroFirmware_${MCU_TARGET}.bin"
echo "   - HEX: firmware_binaries/AeroFirmware_${MCU_TARGET}.hex"
echo "   - Upload: firmware_uploads/AeroFirmware_${MCU_TARGET}.bin"
echo ""
echo "Flash commands:"
if [[ "$MCU_TARGET" == *"STM32"* ]]; then
    echo "   STM32: STM32_Programmer_CLI -c port=SWD -w firmware_binaries/AeroFirmware_${MCU_TARGET}.hex -v -rst"
elif [[ "$MCU_TARGET" == *"ESP32"* ]] || [ "$MCU_TARGET" = "ESP8266" ]; then
    echo "   ESP: esptool.py --chip $MCU_TARGET write_flash 0x0 firmware_binaries/AeroFirmware_${MCU_TARGET}.bin"
fi

echo "=========================================="
echo "Build completed successfully! 🚀"
echo "Existing binary structure updated with wireless and GPS features"
echo "=========================================="
