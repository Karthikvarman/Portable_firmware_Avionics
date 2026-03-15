# AeroFirmware — Portable Real-Time Avionics Firmware

[![License: MIT](https://img.shields.io/badge/License-MIT-blue)](LICENSE)
[![Target](https://img.shields.io/badge/MCU-STM32H7%20%7C%20F4%20%7C%20F7%20%7C%20L0%20%7C%20L4%20%7C%20ESP32%20%7C%20ESP8266-green)]()
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-orange)]()

A **complete, production-grade, portable real-time avionics firmware** designed to run on a wide range of embedded MCUs. Implements a 15-state Extended Kalman Filter (EKF), a full Sensor Abstraction Layer (SAL), automatic communication protocol detection, and a professional web-based Ground Station UI.

---

## ✈️ Features

| Feature | Details |
|---|---|
| **Portability** | STM32H7/F4/F7/L0/L4, ESP32, ESP8266 |
| **RTOS** | FreeRTOS 10.x — 7 tasks, mutex-protected shared state |
| **EKF** | 15-state INS (pos/vel/quat/accel bias/gyro bias), NIS health monitoring |
| **Sensors** | LPS25HB, SHT40, LSM6DSV32XTR, LIS2MDLTR, Teseo-LIV4F, TGS-5141 |
| **Protocols** | Auto-detect I2C / SPI / UART / ADC per sensor |
| **Telemetry** | Binary (CRC-16) + ASCII NMEA-style frames @ 20 Hz |
| **Ground Station** | Web UI with AHI, live charts, EKF state table, flash tool |
| **Backend** | FastAPI server with WebSocket serial bridge, REST flash API |

---

## 📁 Project Structure

```
PF-1/
├── config/
│   └── aero_config.h          ← Master configuration (MCU, sensors, RTOS, EKF)
│
├── hal/
│   ├── include/               ← Platform-agnostic HAL headers
│   │   ├── hal_common.h       ← Status codes, types, macros
│   │   ├── hal_i2c.h
│   │   ├── hal_spi.h
│   │   ├── hal_uart.h
│   │   ├── hal_adc.h
│   │   ├── hal_gpio.h
│   │   ├── hal_timer.h
│   │   ├── hal_flash.h
│   │   └── hal_wdt.h
│   ├── stm32h7/               ← STM32H7 LL-driver implementations
│   │   ├── stm32h7_i2c.c
│   │   ├── stm32h7_spi.c
│   │   ├── stm32h7_uart.c
│   │   ├── stm32h7_adc.c
│   │   ├── stm32h7_gpio.c
│   │   ├── stm32h7_timer.c
│   │   └── stm32h7_flash.c
│   └── esp32/                 ← ESP-IDF driver implementations
│       ├── esp32_i2c.c
│       ├── esp32_spi.c
│       ├── esp32_uart.c
│       ├── esp32_adc.c
│       ├── esp32_gpio.c
│       └── esp32_timer.c
│
├── platform/
│   ├── platform.h             ← Platform abstraction interface
│   ├── stm32h7/stm32h7_platform.c   (SYSCLK = 480 MHz)
│   ├── stm32f4/stm32f4_platform.c   (SYSCLK = 168 MHz)
│   ├── stm32f7/stm32f7_platform.c   (SYSCLK = 216 MHz)
│   ├── stm32l4/stm32l4_platform.c   (SYSCLK = 80 MHz, MSI PLL)
│   ├── stm32l0/stm32l0_platform.c   (SYSCLK = 32 MHz, ultra-LP)
│   ├── esp32/esp32_platform.c
│   └── esp8266/esp8266_platform.c
│
├── sal/
│   ├── include/sal_sensor.h   ← Sensor Abstraction Layer public API
│   ├── sal_sensor.c           ← Driver registry, health tracking
│   └── drivers/
│       ├── lps25hb/           ← Barometer (I2C/SPI)
│       ├── sht40/             ← Temp+Humidity (I2C, CRC)
│       ├── lsm6dsv/           ← IMU Accel+Gyro (I2C/SPI)
│       ├── lis2mdl/           ← Magnetometer (I2C/SPI, hard-iron cal)
│       ├── teseo_liv4f/       ← GNSS (UART/I2C, NMEA parser)
│       └── tgs5141/           ← CO Gas (ADC, TIA model, warmup timer)
│
├── core/
│   ├── ekf/
│   │   ├── aero_ekf.h         ← 15-state EKF API
│   │   └── aero_ekf.c         ← EKF: propagation + baro/GPS/mag updates
│   ├── app/
│   │   ├── main.c             ← Portable entry point
│   │   ├── app_tasks.c        ← All 7 FreeRTOS tasks
│   │   └── app_tasks.h
│   ├── telemetry/
│   │   ├── telemetry.c        ← Binary CRC-16 + ASCII NMEA frames
│   │   └── telemetry.h
│   └── protocol/
│       ├── protocol_manager.c ← Auto-detect I2C/SPI/UART per sensor
│       └── protocol_manager.h
│
├── rtos/
│   ├── FreeRTOSConfig.h       ← FreeRTOS tuned for STM32H7
│   ├── freertos_hooks.c       ← Stack overflow, malloc fail, assert
│   └── os_api/
│       ├── os_api.h           ← RTOS abstraction layer
│       └── os_api_freertos.c  ← FreeRTOS implementation
│
├── build/
│   ├── CMakeLists.txt         ← Multi-MCU CMake build
│   ├── toolchains/stm32.cmake ← ARM GCC toolchain file
│   └── ld/stm32h743_flash.ld  ← STM32H743 linker script
│
├── ui/
│   ├── index.html             ← Ground Station UI (4 tabs)
│   ├── style.css              ← Dark avionics theme
│   └── app.js                 ← AHI, charts, EKF table, flash workflow
│
├── tools/
│   └── uart_rx.py             ← CLI serial receiver with ANSI dashboard
│
├── server.py                  ← FastAPI WebSocket bridge + flash API
└── requirements.txt           ← Python dependencies
```

---

## 🚀 Build Instructions

### Prerequisites
- **ARM GCC Toolchain** ≥ 13.x: https://developer.arm.com/downloads
- **CMake** ≥ 3.20
- **Python** ≥ 3.11 (for ground station server)
- **STM32 Vendor Libs**: STM32H7 LL drivers, CMSIS, FreeRTOS (place in `vendor/`)

### STM32H7

```bash
# From project root
cmake -B build/stm32h7 \
      -DCMAKE_TOOLCHAIN_FILE=build/toolchains/stm32.cmake \
      -DTARGET_MCU=STM32H7 \
      -DCMAKE_BUILD_TYPE=Release \
      build/

cmake --build build/stm32h7 --parallel

# Flash via ST-LINK
cmake --build build/stm32h7 --target flash
```

### STM32F4 / F7 / L4 / L0

```bash
cmake -B build/stm32f4 -DTARGET_MCU=STM32F4 build/
cmake --build build/stm32f4 --parallel
```

### ESP32 (ESP-IDF)

```bash
idf.py set-target esp32
idf.py menuconfig   # Set AERO_TARGET_ESP32 in component config
idf.py build
idf.py flash monitor
```

---

## 🖥️ Ground Station UI

### Web UI (Standalone — no server required)
Simply open `ui/index.html` in Chrome/Edge:
- Demo mode starts automatically (simulated flight data)
- **Flash tab**: upload `.hex`/`.bin`, select MCU, flash with progress
- **Monitor tab**: real-time sensor cards, live time-series chart
- **Attitude tab**: Artificial Horizon Indicator (canvas-rendered)
- **EKF State tab**: 15-state vector, covariance, innovation monitor

### Full Backend (Real Serial Port)
```bash
pip install -r requirements.txt
python server.py --port COM3 --baud 921600

# Or with auto-connect:
python server.py --serial COM3 --baud 921600 --port 8001
# Then open http://localhost:8001
```

### CLI Serial Receiver
```bash
python tools/uart_rx.py --port COM3 --baud 921600 --csv flight_log.csv
```

---

## 🧠 EKF State Vector (15 States)

| # | State | Symbol | Units |
|---|---|---|---|
| 0-2 | Position NED | p_N, p_E, p_D | m |
| 3-5 | Velocity NED | v_N, v_E, v_D | m/s |
| 6-9 | Attitude Quaternion | q_w, q_x, q_y, q_z | — |
| 10-12 | Accelerometer Bias | ba_x, ba_y, ba_z | m/s² |
| 13-14 | Gyroscope Bias | bg_x, bg_y | rad/s |

**Fused Sensors:** IMU @ 200 Hz · Barometer @ 50 Hz · Magnetometer @ 50 Hz · GNSS @ 10 Hz

**Health Monitoring:** Normalized Innovation Squared (NIS) with χ² test (p=0.05)

---

## 📡 Telemetry Frame Format

### Binary (default, 20 Hz)
```
[0xAE][LEN_L][LEN_H][TYPE=0x01][PAYLOAD 64B][CRC16_H][CRC16_L][0xEA]
```
CRC: CRC-16/CCITT (poly 0x1021, init 0xFFFF)

### ASCII NMEA-style
```
$AERO,<tick_ms>,<roll>,<pitch>,<yaw>,<alt_m>,<lat>,<lon>,<sats>,<co_ppm>*<XOR_HEX>\r\n
```

---

## ⚙️ Configuration (`config/aero_config.h`)

```c
// Select target MCU (only one)
#define AERO_TARGET_STM32H7

// Enable sensors
#define AERO_SENSOR_LPS25HB_ENABLE   1
#define AERO_SENSOR_SHT40_ENABLE     1
#define AERO_SENSOR_LSM6DSV_ENABLE   1
#define AERO_SENSOR_LIS2MDL_ENABLE   1
#define AERO_SENSOR_TESEO_LIV4F_ENABLE 1
#define AERO_SENSOR_TGS5141_ENABLE   1

// Interface auto-selection (or force: AERO_IFACE_I2C / AERO_IFACE_SPI)
#define AERO_LPS25HB_IFACE    AERO_IFACE_AUTO
// ...

// EKF noise parameters
#define AERO_EKF_Q_ACCEL      1e-4f
#define AERO_EKF_R_BARO_ALT   0.5f   // [m] baro measurement noise
#define AERO_EKF_R_GPS_POS    3.0f   // [m] GPS position noise
```

---

## 📋 FreeRTOS Tasks

| Task | Priority | Period | Function |
|---|---|---|---|
| `tsk_imu` | SENSOR (6) | 5 ms | IMU read + EKF propagation at 200 Hz |
| `tsk_baro` | SENSOR (6) | 20 ms | Baro read + EKF altitude update at 50 Hz |
| `tsk_mag` | SENSOR (6) | 20 ms | Mag read + EKF heading update at 50 Hz |
| `tsk_gnss` | SENSOR (6) | 100 ms | GNSS read + EKF position/velocity update at 10 Hz |
| `tsk_env` | SENSOR (6) | 500 ms | SHT40 + CO gas at 1–2 Hz |
| `tsk_telem` | TELEMETRY (5) | 50 ms | Build + transmit telemetry frames at 20 Hz |
| `tsk_mon` | MONITOR (3) | 1000 ms | Sensor health re-init + WDT kick |

---

## 📄 License

MIT License — see [LICENSE](LICENSE)

---

*Built with ❤️ for avionics applications. Designed for reliability, portability, and real-time performance.*
