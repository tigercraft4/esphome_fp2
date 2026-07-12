# External Integrations

**Analysis Date:** 2026-07-12

## APIs & External Services

**Home Assistant:**
- **ESPHome Native API** - Device-to-HA encrypted communication
  - SDK/Client: Built into ESPHome framework
  - Auth: API encryption key (`!secret api_encryption_key`) - generated via `openssl rand -base64 32`
  - Port: Default 6053 (ESPHome protocol)
  - Encryption: AES-128 (key from secrets)
  - Custom Actions: Device exposes RPC endpoints for:
    - `get_map_config` - Returns radar map data as JSON
    - `fp2_force_detection_config` - Force presence/live-map setup
    - `fp2_configure_sleep_mode` - Configure sleep detection zone
    - `fp2_set_work_mode` - Switch between work modes (3=presence, 9=sleep)
    - `fp2_calibrate_empty_room` - Empty-room calibration
    - `fp2_reset_radar` - Radar module soft reset
    - `fp2_write_attr_uint8/16/32/bool` - Raw attribute write probes (reverse-engineering)
    - `fp2_read_detection_config`, `fp2_read_mode_calibration_config`, `fp2_read_attr` - Diagnostic reads

**HACS (Home Assistant Community Store):**
- **Card Distribution** - Custom Lovelace card distribution
  - Repository: GitHub (https://github.com/tigercraft4/esphome_fp2)
  - Metadata: `hacs.json` (filename: `card.js`, render_readme: false)
  - Installation: Via HACS frontend UI or manual copy
  - Card Availability: Once published to HACS default or as custom repository

## Data Storage

**Databases:**
- None configured (stateless sensor device)

**File Storage:**
- **Local Filesystem (FP2 device):**
  - Firmware image (flash storage)
  - Configuration cache (SPIFFS or NVS)
  - Calibration data (unit-specific, backup recommended before flashing)

**Caching:**
- **FreeRTOS Task Queues** - Radar report buffering
- **Text Sensor State** - Latest radar reports cached in Home Assistant entities

**Home Assistant Storage:**
- Entity state history (standard HA database)
- Custom card JavaScript cache (browser-side)

## Authentication & Identity

**Auth Provider:**
- **Custom ESPHome API** - Symmetric key encryption
  - Implementation: API encryption key (shared secret) - stored in secrets.yaml
  - WiFi WPA2/WPA3 - Standard WiFi authentication
  - OTA Update Password - Plain password auth for firmware updates (`!secret ota_password`)

**WiFi:**
- **Primary Network:** SSID and password from secrets (`!secret wifi_ssid`, `!secret wifi_password`)
- **Fallback AP (Access Point):** 
  - SSID: "FP2 Fallback"
  - Password: `!secret ap_password`
  - Triggered: When primary WiFi unavailable for 15+ minutes
- **Captive Portal:** Enabled (redirects HTTP requests to config page if not connected)

## Monitoring & Observability

**Error Tracking:**
- None configured (no external error service)

**Logs:**
- **Local Device Logs:** ESPHome native logging (serial UART + network log sink)
  - Level: INFO (configurable)
  - Output: Home Assistant Logs integration (real-time)
- **Radar Debug Stream:** Text sensor (`radar_debug`) publishes:
  - Unhandled/unknown reports
  - Command TX/ACK/retry/timeout telemetry
  - Thermodynamic chart raw BLOB (when enabled)
- **Logger Diagnostic Entities:**
  - `radar_temperature` - Sensor temperature (°C)
  - `radar_software_version` - Firmware version string
  - `people_counting` - Text sensor with id/ontime/realtime counts

## CI/CD & Deployment

**Hosting:**
- **GitHub** - Source code repository
  - URL: https://github.com/tigercraft4/esphome_fp2
  - Branch: `main` (default for external component source)

**OTA (Over-The-Air) Updates:**
- **Platform:** ESPHome OTA protocol
  - Configuration: `ota:` block with password-protected updates
  - Password: `!secret ota_password`
  - Mechanism: Post-firmware built locally/remotely, uploaded via ESPHome API/CLI
  - Requirement: Device must be online and ESPHome integration active

**CI Pipeline:**
- None configured (manual build/test expected)

**Component Distribution:**
- **ESPHome External Components:**
  - Git source: `https://github.com/tigercraft4/esphome_fp2` (ref: `main`)
  - Auto-download during ESPHome compile
  - Refresh interval: Optional (default 24h)

- **Home Assistant Card (HACS):**
  - Distribution: `card.js` served via HACS CDN or manual copy
  - Installation path: `/local/community/aqara-fp2-card/card.js`
  - Lovelace resource type: `module` (ES6 JavaScript module)

## Environment Configuration

**Required Environment Variables (via secrets.yaml):**
- `wifi_ssid` - WiFi network name
- `wifi_password` - WiFi network password
- `api_encryption_key` - 32-byte base64 string (generate: `openssl rand -base64 32`)
- `ota_password` - OTA update password (any string)
- `ap_password` - Fallback AP password (any string)

**Device Configuration Variables (YAML):**
- `mounting_position` - Enum: `wall`, `left_corner`, `right_corner` (affects target tracking coordinates)
- `left_right_reverse` - Boolean: Mirror X-axis (for rotated mount)
- `presence_sensitivity` - Enum: `low`, `medium`, `high` (per-zone configurable)
- `motion_timeout` - Duration: Motion debounce hold time (default 5s)
- `zone_type` - Enum: `tv`, `green_plant`, `leisure`, `dressing`, `closet`, `desk`, `shower`, `stairs` (furniture/scene for false-positive filtering)

**Firmware-Derived Options:**
- `people_counting_report_enable` - Enable people-counting reports (0x0158)
- `people_number_enable` - Enable current/ontime people counts (0x0162)
- `target_type_enable` - AI person detection (0x0163) - may suppress target streaming
- `dwell_time_enable` - Enable dwell-time reports (0x0172)
- `walking_distance_enable` - Enable walking-distance reports (0x0173)
- `thermodynamic_chart_enable` - Enable thermodynamic chart data (0x0138)
- `sleep_report_enable` - Enable sleep detection (0x0156)
- `posture_report_enable` - Enable target posture reports (0x0157)
- `fall_detection` - Enable fall detection (0x0121)
- `fall_detection_sensitivity` - Fall detection sensitivity: `low`, `medium`, `high` (0x0123)

**Secrets Location:**
- File: `secrets.yaml` (gitignored, must be created from `secrets.yaml.example`)
- Safe: Never committed to version control

## Hardware Communication Protocols

**UART (Aqara FP2 Radar):**
- **Interface:** Direct serial UART to mmWave radar module
- **Pins:** TX=GPIO19, RX=GPIO18
- **Speed:** 890000 baud (non-standard, FP2-specific)
- **Protocol:** Binary packet format with CRC16-MODBUS checksums
  - Packet structure: [Header (0xFD 0xFC)] + [Length] + [Data] + [CRC16]
  - Report types (SubIDs): Location tracking (0x0117), people counting (0x0155-0x0166), sleep data (0x0159), etc.
  - Attributes (0x0100-0x01FF): Radar configuration registers (write via probe actions)

**I2C (Accelerometer & Illuminance):**
- **Interface:** Standard I2C bus
- **Pins:** SDA=GPIO33, SCL=GPIO32
- **Speed:** 100 kHz
- **Devices:**
  - **Accelerometer (0x27):** Mounting orientation detection via `aqara_fp2_accel` component
    - Detected axes: X, Y, Z acceleration (for up/down orientation)
  - **OPT3001 (0x44):** Illuminance sensor (built-in ESPHome platform)
    - Measurement: Ambient light (lux)
    - Update interval: 30s (configurable)

**GPIO (LED & Button):**
- **RGB Status LED:** 
  - Pins: R=GPIO14, G=GPIO26, B=GPIO27 (all inverted)
  - Control: LEDC PWM output via light/rgb platform
  - States: On-boot indication, connectivity, data transfer feedback
- **Device Button:**
  - Pin: GPIO36 (inverted)
  - Usage: Manual trigger for calibration/reset actions (binary sensor)

## Webhooks & Callbacks

**Incoming Webhooks:**
- None configured

**Outgoing Webhooks:**
- None configured (one-way push to Home Assistant via ESPHome API)

**Event Streams:**
- **Radar Reports:** Continuous binary stream (UART) → Decoded into entities
  - Report 0x0117: Location tracking (target position, velocity)
  - Report 0x0155-0x0166: People counting (realtime, accumulated)
  - Report 0x0159: Sleep detection data
  - Report 0x0154: Target posture
  - Report 0x0115: Zone motion events (momentary, debounced)

## Home Assistant Integration Points

**Entities Exposed:**
- **Binary Sensors:** Global/zone presence, sleep presence, device button, zone motion
- **Text Sensors:** Target tracking (JSON), radar debug, people counting, radar version, sleep state, target posture
- **Sensors:** Realtime/ontime people numbers, people counting, walking distance, radar temperature, illuminance, target count, nearest distance
- **Switches:** Location report toggle (live-view enable/disable)
- **Lights:** RGB status LED control
- **Buttons:** Force config, calibrate, reset radar
- **Services:** Custom actions (get_map_config, fp2_* probe actions)

**Custom Card:**
- **Entity Mapping:** Requires entity_prefix (e.g., `sensor.fp2_living_room`)
- **Services Called:** `esphome.{device_name}_get_map_config` (JSON response)
- **State Subscriptions:** Target tracking text sensor, mounting position entity
- **Display:** 14×14 grid rendering with zones, targets, interference overlay

---

*Integration audit: 2026-07-12*
