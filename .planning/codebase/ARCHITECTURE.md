<!-- refreshed: 2026-07-12 -->
# Architecture

**Analysis Date:** 2026-07-12

## System Overview

```text
┌─────────────────────────────────────────────────────────────┐
│              Home Assistant Integration Layer                │
│  Entity Publishing (sensors, binary sensors, switches, text) │
│              `components/aqara_fp2/__init__.py`              │
└─────────────────────────────────────────────────────────────┘
                             ▲
                             │ Entities published
                             │
┌─────────────────────────────────────────────────────────────┐
│                    C++ Runtime Layer                         │
│  FP2Component + FP2Zone + Protocol Handling (UART)          │
│  `components/aqara_fp2/fp2_component.cpp/h`                │
│  `components/aqara_fp2_accel/aqara_fp2_accel.cpp/h`        │
└─────────────────────────────────────────────────────────────┘
         ▲                                    ▲
         │ UART UART frames (REPORT/ACK)    │ I2C polling
         │                                   │
┌────────┴──────────────────────────────────┴────────────────┐
│  Hardware Layer (FP2 mmWave Sensor + Accelerometer)         │
│  - UART @ GPIO18/19, 890000 baud                            │
│  - I2C @ GPIO32/33, 100kHz (accel @ 0x27, illum @ 0x44)   │
└─────────────────────────────────────────────────────────────┘
         │
         │ YAML Configuration
         │
┌─────────────────────────────────────────────────────────────┐
│         Python Component Codegen & Validation Layer          │
│    Config validation → C++ code generation                   │
│    `components/aqara_fp2/__init__.py`                       │
│    `components/aqara_fp2_accel/__init__.py`                 │
└─────────────────────────────────────────────────────────────┘
         ▲
         │ Device Config (YAML)
         │
┌─────────────────────────────────────────────────────────────┐
│          Configuration & Visualization Layer                 │
│  Device YAML configs + Home Assistant card.js               │
│  `fp2-sala.yaml`, `example_config.yaml`, `card.js`          │
└─────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| **FP2Component** | Main UART communication, protocol parsing, radar control, zone management, attribute read/write | `components/aqara_fp2/fp2_component.cpp/h` |
| **FP2Zone** | Zone grid definition, sensitivity settings, presence/motion reporting per zone | `components/aqara_fp2/fp2_component.h` (lines 29-105) |
| **FP2LocationSwitch** | Toggle live-view target reporting on/off (location_report_switch entity) | `components/aqara_fp2/fp2_component.h` |
| **AqaraFP2Accel** | I2C polling of accelerometer at 0x27, mounting position detection | `components/aqara_fp2_accel/aqara_fp2_accel.cpp/h` |
| **Python Config Layer** | YAML validation, grid parsing, sensor/binary_sensor/switch entity generation | `components/aqara_fp2/__init__.py` |
| **HA Card** | Real-time visualization of targets, zones, grid overlays, interactive controls | `card.js` |

## Pattern Overview

**Overall:** ESPHome Custom Component + Home Assistant Integration

**Key Characteristics:**
- UART-based communication with FP2 mmWave sensor using proprietary protocol
- Configuration-driven entity generation (sensors, binary sensors, text sensors, switches)
- Zone-based detection with customizable 14×14 detection grids
- Reverse-engineered firmware attribute mapping (SubID protocol)
- Firmware-derived optional features (sleep reports, posture detection, fall detection)
- Grid ASCII → binary conversion at compile-time
- Real-time target tracking published as JSON text sensor
- I2C accelerometer for mounting position inference

## Layers

**Configuration & Validation Layer:**
- Purpose: Parse YAML device config, validate grids/zones, map configuration to C++ code generation
- Location: `components/aqara_fp2/__init__.py`, `components/aqara_fp2_accel/__init__.py`
- Contains: CONFIG_SCHEMA definitions, grid parsing (`parse_ascii_grid`), SENSOR_MAP/ZONE_SENSOR_MAP routing, async `to_code()` generators
- Depends on: ESPHome codegen framework, config validation module
- Used by: ESPHome compiler during firmware build

**Python Entity Generation Layer:**
- Purpose: Create entity definitions (sensors, binary sensors, text sensors, switches) from config
- Location: `components/aqara_fp2/__init__.py` (lines 327-352)
- Contains: SENSOR_MAP (14 mappings), ZONE_SENSOR_MAP (3 mappings), entity instantiation in `to_code()`
- Depends on: esphome.components (binary_sensor, sensor, switch, text_sensor, uart)
- Used by: to_code() coroutine during build

**C++ Runtime Layer:**
- Purpose: Handle UART protocol, parse radar reports, manage zones, expose control actions, publish sensor updates
- Location: `components/aqara_fp2/fp2_component.cpp/h`, `components/aqara_fp2_accel/aqara_fp2_accel.cpp/h`
- Contains: 
  - UART frame parsing (OpCode: READ, WRITE, RESPONSE, REPORT, ACK)
  - Protocol handlers for 40+ radar SubIDs (0x0101 - 0x0201)
  - Motion debouncing per zone
  - Grid-based target filtering
  - Attribute write probes for reverse engineering
  - CRC16-MODBUS checksum
- Depends on: ESPHome core (UART, Component), Arduino JSON library, standard C++
- Used by: ESPHome runtime on device

**Entity Publishing Layer:**
- Purpose: Update Home Assistant entities based on parsed radar data
- Location: FP2Component methods (set_*_sensor, publish_state calls in fp2_component.cpp)
- Contains: BinarySensor, Sensor, TextSensor, Switch pointers and publish calls
- Depends on: ESPHome entity classes (BinarySensor, Sensor, TextSensor, Switch)
- Used by: HA via ESPHome API

**Visualization Layer:**
- Purpose: Real-time 14×14 grid rendering, target tracking, zone occupancy, interactive controls
- Location: `card.js`
- Contains: HTML canvas rendering, grid overlay drawing, target velocity vectors, zone color coding
- Depends on: Home Assistant Lovelace card API, TextSensor entity state (targets JSON)
- Used by: Home Assistant Lovelace dashboard

## Data Flow

### Primary Request Path: UART Frame Reception → Entity Update

1. **FP2 Hardware sends UART frame** (no explicit entry point, continuous async)
   - Frame format: `[header][op_code][attr_id/subid][data_len][payload][crc16]`
   - OpCode 0x05 (REPORT) for async radar data, 0x01 (RESPONSE) for read replies

2. **FP2Component::loop() polls UART** (`fp2_component.cpp`)
   - Reads available bytes from UART, buffers into frame
   - On complete frame (CRC16 match): calls `handle_report_frame()` or `handle_response_frame()`

3. **Report handler parses SubID and payload** (`fp2_component.cpp`)
   - Examples:
     - SubID 0x0104: presence_detect → publish global presence sensor
     - SubID 0x0115: zone_motion → publish zone motion (with debounce)
     - SubID 0x0164: realtime_people_number → publish counter sensor
     - SubID 0x0201: radar_debug unhandled + telemetry

4. **Zone grid matching (if enabled)** (`fp2_component.cpp`)
   - For motion/presence: test target coordinates against zone GridMaps
   - GridMap = 40-byte array representing 20×16 bits, active area is centered 14×14
   - Bit set = active detection cell

5. **Entity state published** (`fp2_component.cpp`)
   - `zone->publish_presence(bool)` → BinarySensor::publish_state()
   - `zone->publish_motion(bool)` → motion debouncing via `tick_motion()` (5s default timeout)
   - `radar_temperature_sensor_->publish_state(float)` → Sensor
   - Target JSON → `target_tracking_sensor_->publish_state(string)` → TextSensor

6. **ESPHome API transmits to HA** (framework handles)
   - Publish events → Home Assistant entity state updates

### Zone Motion Debounce Flow

1. Motion event (0x0115) received → `zone->note_motion_event(now)` sets `motion_active = true`, publishes ON
2. No new motion event for 5000ms → `zone->tick_motion(now)` checks timeout, publishes OFF
3. Prevent immediate re-trigger with debounce window

### Configuration-Time Grid Processing

1. **YAML parsing**: `interference_grid: |-` ASCII art string
2. **`parse_ascii_grid()`**: 14 lines × 14 chars (x/X = active, ./space = inactive) → 40-byte array
   - Maps input 14×14 centered in 20×16 protocol grid
   - Uses Big-Endian bit ordering per row
3. **Compile-time storage**: Grid hex string stored in C++ const string via `grid_to_hex_string()`
4. **Map config JSON**: All grids/zones serialized to JSON at compile time, exposed via `json_get_map_data()` action for HA card

### Configuration Service → Action Flow

1. **HA calls ESPHome action** (example: `get_map_config`)
2. **Action handler** (defined in `example_config.yaml` lines 31-36):
   ```yaml
   - action: get_map_config
     supports_response: only
     then:
       - api.respond:
           data: !lambda |-
             id(fp2).json_get_map_data(root);
   ```
3. **Lambda calls C++ method** `FP2Component::json_get_map_data(JsonObject& root)`
4. **C++ serializes and responds** with JSON containing mounting_position, grids, zones
5. **HA Card receives**, parses, renders visualization

**State Management:**
- **Runtime state**: FP2Component holds zone pointers, current targets, recent motion timestamps
- **Configuration state**: Immutable after compile (grids, sensitivity levels, zone IDs)
- **Entity state**: Managed by ESPHome entity classes (BinarySensor, Sensor, TextSensor, Switch hold current state)
- **HA entity history**: Managed by Home Assistant (no bidirectional sync back to device)

## Key Abstractions

**GridMap:**
- Purpose: Represent 14×14 detection grid in protocol-compatible 40-byte binary format
- Examples: `std::array<uint8_t, 40>` in `fp2_component.h` line 27
- Pattern: Parsed from ASCII at compile time, byte-accurate for protocol compatibility

**FP2Zone:**
- Purpose: Encapsulate per-zone detection logic (grid, sensitivity, motion debounce)
- Examples: `components/aqara_fp2/fp2_component.h` lines 29-105
- Pattern: Struct with grid + sensitivity + motion state machine, published via binary_sensor pointers

**Protocol Enums:**
- Purpose: Map firmware binary codes to semantic meaning
- Examples: `OpCode` (READ=0x04, WRITE=0x02, REPORT=0x05, ACK=0x03), `DataType` (UINT8, UINT16, UINT32), `AttrId` (RADAR_SW_VERSION=0x0102, PRESENCE_DETECT=0x0104, etc.)
- Pattern: Used in frame parsing switch statements, bidirectional encode/decode

**FP2LocationSwitch:**
- Purpose: Enable/disable location reporting (target tracking) via switch entity
- Pattern: Custom switch class controlling `LOCATION_REPORT_ENABLE` (0x010A) attribute
- Usage: `location_report_switch:` in YAML creates "Report Targets" switch in HA

## Entry Points

**Device Configuration (Static):**
- Location: `fp2-sala.yaml`, `example_config.yaml`
- Triggers: ESPHome compiler reads this file
- Responsibilities: Define UART pins, I2C pins, mounting position, zone grids, sensitivity, enabled report options
- Output: C++ firmware binary with compiled config

**UART Frame Reception (Async):**
- Location: FP2Component::loop() in `fp2_component.cpp` (called by ESPHome event loop every tick)
- Triggers: UART hardware has available bytes
- Responsibilities: Receive frames, buffer, CRC check, dispatch to report/response handlers
- Output: Entity state updates published to HA API

**Action Invocations (Synchronous):**
- Location: ESPHome actions defined in YAML `api:` section
- Examples: `fp2_force_detection_config`, `fp2_set_work_mode`, `get_map_config`, `fp2_calibrate_empty_room`
- Triggers: HA service calls (via UI button, automation, script)
- Responsibilities: Execute lambda that calls FP2Component method (e.g., `force_detection_config()`, `set_work_mode()`)
- Output: UART command frames sent to radar, optional JSON response

**Home Assistant Card (UI):**
- Location: `card.js`
- Triggers: HA Lovelace renders card, user interactions (click, toggle)
- Responsibilities: Fetch map config via `get_map_config` action, poll target tracking text sensor, render canvas visualization
- Output: Visual grid + targets + zones, switch toggles for live view

## Architectural Constraints

- **Threading:** Single-threaded event loop (FreeRTOS UNICORE enabled in ESP32 config)
- **Global state:** FP2Component is singleton, held by ESPHome framework; AqaraFP2Accel is singleton via I2C device registry
- **Circular imports:** None observed; components depend on esphome.* and standard libraries only
- **UART baudrate:** Hardcoded 890000 (non-standard, must match FP2 hardware)
- **GridMap format:** Strictly 40 bytes (20 rows × 2 bytes per row, Big-Endian bit ordering); any deviation breaks protocol
- **I2C addresses:** Accelerometer @ 0x27, OPT3001 illuminance @ 0x44 (hardcoded, no discovery)
- **Protocol compatibility:** Binary frame format is reverse-engineered; firmware updates to FP2 may break compatibility

## Anti-Patterns

### Protocol Handling in C++ Without Abstraction

**What happens:** Raw byte buffers and switch statements on OpCode/DataType/AttrId directly in `handle_report_frame()` / `handle_response_frame()`. New report types require editing the monster switch statement.

**Why it's wrong:** Adding new SubID handlers (e.g., a new sleep mode variant) requires navigating 40+ existing cases. Parsing logic is interleaved with business logic (filtering, debounce, publish).

**Do this instead:** Create a report handler registry pattern:
```cpp
using ReportHandler = std::function<void(const std::vector<uint8_t>& payload)>;
std::map<uint16_t, ReportHandler> report_handlers_;
// Then: report_handlers_[0x0104] = [this](auto& p) { handle_presence_detect(p); };
```

This allows FP2Component to delegate SubID handling to named methods, reducing cyclomatic complexity.

### Grid Parsing at Compile Time vs. Validation at Runtime

**What happens:** `parse_ascii_grid()` is called during config validation (in Python), converting the 14×14 ASCII to bytes. The result is stored as a list in config schema. Then in `to_code()`, it's passed directly to C++ code generation without re-parsing.

**Why it's still OK here:** The conversion is deterministic and happens once at build time. No value mismatch is possible if Python and C++ both use the result.

**Potential concern:** If someone hand-edits a compiled binary's grid constant, there's no validation. Consider: add an integrity marker (e.g., grid version byte) if grids become mutable at runtime.

## Error Handling

**Strategy:** Silent fallback on protocol errors; diagnostic logging to `radar_debug` text sensor

**Patterns:**
- CRC mismatch: discard frame, no retry (UART is continuous; next frame will arrive)
- Unknown OpCode: log to `radar_debug`, skip frame
- Missing required config (e.g., accel missing): build-time error via `cv.Required("accel")`
- Action service not found: ESPHome reports "action not found" to HA
- Zone presence/motion sensors not configured: nullptr check prevents null dereference; `publish_presence/motion()` returns early

## Cross-Cutting Concerns

**Logging:** 
- Framework: ESPHome's `ESP_LOG*` macros (log level: INFO for fp2-sala, DEBUG available)
- Patterns: tag "aqara_fp2", log frame RX/TX, report parsing, zone changes
- Diagnostic output: `radar_debug` text sensor collects unhandled reports, command telemetry

**Validation:**
- Config-time: Python schema validates YAML types, grid format (14 lines, 14 chars), sensitivity enums
- Runtime: GridMap 40-byte size checked implicitly via C++ array template; zone ID range implicit

**Authentication:**
- API encryption: configured via `api: encryption: key: !secret api_encryption_key`
- No per-action auth; ESPHome API is device-to-HA only

---

*Architecture analysis: 2026-07-12*
