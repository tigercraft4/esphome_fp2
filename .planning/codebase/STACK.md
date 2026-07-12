# Technology Stack

**Analysis Date:** 2026-07-12

## Languages

**Primary:**
- **Python** 3.11+ - ESPHome component configuration and metadata (`pyproject.toml` requires `>=3.11`)
- **C++** - Custom component implementation for FP2 radar sensor control and accelerometer interface
- **YAML** - Device configuration and ESPHome settings
- **JavaScript** - Home Assistant visualization card (`card.js`)

**Secondary:**
- **JSON** - HACS metadata, configuration responses, map data serialization (uses ArduinoJson library)

## Runtime

**Environment:**
- **ESP32-SOLO-1** (single-core ESP32 variant) - Stock FP2 hardware
- **ESPHome 2025.12.4+** - Firmware framework and device management

**Package Manager:**
- **pip/poetry** (Python) - ESPHome dependency management via `pyproject.toml`
- **Lockfile:** Not present (uses pyproject.toml with loose version constraint)

## Frameworks

**Core:**
- **ESPHome 2025.12.4+** - Modular firmware framework for embedded devices, handles component lifecycle, UART, I2C, GPIO, sensors, switches
- **ESP-IDF** - Espressif IoT Development Framework (C/C++ SDK) with FreeRTOS
  - Single-core mode enforced via `CONFIG_FREERTOS_UNICORE: "y"`
  - ESP-System single-core mode via `CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE: "y"`
  - Memory optimization: `sram1_as_iram: true` (uses second SRAM bank as instruction RAM)

**Testing:**
- Not detected (no test framework configured)

**Build/Dev:**
- **esptool** - Firmware backup/restore and flashing utility (documented in `FLASHING.md`)
- **ESPHome CLI/Build System** - YAML-driven firmware compilation and OTA deployment
- **ArduinoJson** - C++ JSON library for sensor data serialization (`#include <ArduinoJson.h>`)

## Key Dependencies

**Critical:**
- **esphome >=2025.12.4** - Core framework; defined in `pyproject.toml`
  - Provides: component codegen (`esphome.codegen`), config validation (`esphome.config_validation`), UART/I2C/GPIO/sensor APIs
  - Auto-loaded components: `binary_sensor`, `text_sensor`, `sensor`, `switch`, `json`

**Infrastructure:**
- **ArduinoJson** - C++ JSON serialization for radar map data and responses
- **Home Assistant ESPHome Integration** - Receives device data via ESPHome native API
- **OPT3001** - Sensor component for illuminance measurement (built-in ESPHome platform)
- **FreeRTOS** - Real-time OS kernel (via ESP-IDF)

## Configuration

**Environment:**
- **Secrets Management:** `.yaml` files using `!secret` YAML tag (e.g., `!secret wifi_ssid`)
  - Template: `secrets.yaml.example` (committed; actual `secrets.yaml` is gitignored)
  - Required secrets:
    - `wifi_ssid` - WiFi network name
    - `wifi_password` - WiFi password
    - `api_encryption_key` - Generated via `openssl rand -base64 32`
    - `ota_password` - OTA update password
    - `ap_password` - Fallback AP password

**Device Configuration Files:**
- **`fp2-sala.yaml`** - Production device configuration for "sala" (living room) instance
  - Defines radar zones, sensitivity levels, sensor entities, GPIO pins, UART/I2C setup
- **`example_config.yaml`** - Template configuration with full feature set (local components)
  - Documents all available configuration options and firmware-derived radar controls

**Build Configuration:**
- **ESP32-SOLO-1 Board Settings:**
  - Framework: `esp-idf` (not Arduino)
  - CPU: Single-core enforced (FP2 hardware limitation)
  - Memory: `sram1_as_iram: true` (required for firmware fit)
  - MAC address handling: `ignore_efuse_mac_crc: true`, `ignore_efuse_custom_mac: true`

**Logging:**
- **Level:** INFO (configurable in device YAML)
- **Output:** ESPHome native logging (serial + network)

## Hardware Interfaces

**UART:**
- **Bus ID:** `uart_bus`
- **TX Pin:** GPIO19
- **RX Pin:** GPIO18
- **Baud Rate:** 890000 (FP2 radar protocol speed)
- **Purpose:** Direct communication with Aqara FP2 mmWave radar chip

**I2C:**
- **SDA Pin:** GPIO33
- **SCL Pin:** GPIO32
- **Frequency:** 100 kHz
- **Devices:**
  - Accelerometer (address 0x27) - Mounting orientation detection
  - OPT3001 illuminance sensor (address 0x44) - Ambient light measurement
- **Scan:** Enabled (device discovery)

**GPIO Outputs:**
- **LED (RGB):**
  - Red: GPIO14 (inverted)
  - Green: GPIO26 (inverted)
  - Blue: GPIO27 (inverted)
- **Radar Reset:** GPIO13 (FP2 hardware reset line)

**GPIO Inputs:**
- **Device Button:** GPIO36 (inverted, on-device physical button)

## Web Server

**Platform:** ESPHome embedded web server
- **Port:** 80 (HTTP)
- **Version:** 2 (embedded/offline-friendly for ESP32-SOLO-1 resource constraints)
- **Purpose:** Local diagnostics UI accessible at `http://<device-ip>/`
- **Authentication:** Optional (can add `auth:` block in config)

## Component Architecture

**Custom ESPHome Components:**
- **`aqara_fp2`** (`components/aqara_fp2/`)
  - Implementation: `fp2_component.cpp` / `fp2_component.h`
  - Config: `__init__.py` (YAML schema validation, codegen)
  - Dependencies: UART, binary_sensor, text_sensor, sensor, switch, json
  - Exposes: Target tracking, zone presence/motion, radar diagnostics, firmware-derived features

- **`aqara_fp2_accel`** (`components/aqara_fp2_accel/`)
  - Implementation: `aqara_fp2_accel.cpp` / `aqara_fp2_accel.h`
  - Config: `__init__.py`
  - Purpose: I2C accelerometer interface for mounting position detection

**Home Assistant Card:**
- **`card.js`** - Custom Lovelace card for FP2 visualization
  - Renders: 14×14 grid with target tracking, zones, interference sources
  - Installed via: HACS (recommended) or manual copy to `config/www/`
  - HACS Metadata: `hacs.json` (name: "FP2", entry point: `card.js`)

## Deployment Target

**Production Environment:**
- **Device:** Aqara FP2 Presence Sensor (stock hardware: ESP32-SOLO-1 + mmWave radar module)
- **Flashing:** ESPHome CLI / esptool (see `FLASHING.md`)
  - Requires hardware UART connection (serial header pins + test points)
  - OTA updates: Supported via ESPHome API once initial firmware loaded
- **Home Assistant Integration:** ESPHome native API (encrypted, device-to-HA communication)
- **Hosting:** Distributed via GitHub (component source), HACS (card distribution)

---

*Stack analysis: 2026-07-12*
