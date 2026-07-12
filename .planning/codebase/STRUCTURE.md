# Codebase Structure

**Analysis Date:** 2026-07-12

## Directory Layout

```
esphome_fp2/
├── .git/                      # Git repository metadata
├── .gitignore                 # Git ignore rules (excludes .env, build artifacts)
├── .planning/                 # GSD planning documents (created post-analysis)
├── .python-version            # Python version for development (likely 3.10+)
├── components/                # ESPHome custom components (main deliverable)
│   ├── aqara_fp2/             # Main FP2 radar component
│   │   ├── __init__.py        # Python component definition, config schema, code generation
│   │   ├── fp2_component.h    # C++ header: FP2Component class, FP2Zone, protocol enums
│   │   └── fp2_component.cpp  # C++ implementation: UART, frame parsing, entity handling
│   └── aqara_fp2_accel/       # I2C accelerometer component
│       ├── __init__.py        # Python component definition for accelerometer
│       ├── aqara_fp2_accel.h  # C++ header for AqaraFP2Accel class
│       └── aqara_fp2_accel.cpp # C++ implementation: I2C polling
├── card.js                    # Home Assistant Lovelace visualization card (JavaScript)
├── hacs.json                  # HACS integration metadata
├── example_config.yaml        # Template ESPHome device configuration (full feature set)
├── fp2-sala.yaml              # Real device config (production instance, "sala" = living room)
├── secrets.yaml.example       # Template for secrets (wifi_ssid, api_encryption_key, etc.)
├── fp2-card-test.html         # Standalone HTML test page for card.js
├── card.js                    # Card visualization component
├── README.md                  # Project overview, feature list, installation instructions
├── FLASHING.md                # Device flashing guide (esptool commands, bootloader setup)
├── pyproject.toml             # Python project metadata (minimal)
├── uv.lock                    # Uv package manager lock file (dev dependencies)
└── images/                    # Screenshot and diagram assets
    └── card_screenshot.png    # HA card UI screenshot
```

## Directory Purposes

**`components/aqara_fp2/`:**
- Purpose: Main FP2 radar sensor component — UART communication, zone management, entity generation
- Contains: Python config validation + code generation, C++ runtime for protocol handling, frame parsing
- Key files:
  - `__init__.py`: CONFIG_SCHEMA (YAML validation), grid parser, entity route maps, `to_code()` generator
  - `fp2_component.h`: FP2Component class definition, FP2Zone struct, protocol enums (OpCode, DataType, AttrId)
  - `fp2_component.cpp`: UART loop, frame handling, SubID dispatch, CRC16, entity publish

**`components/aqara_fp2_accel/`:**
- Purpose: I2C accelerometer interface for mounting position detection
- Contains: Minimal Python config, C++ I2C polling at 0x27, mount orientation inference
- Key files:
  - `__init__.py`: CONFIG_SCHEMA, minimal — no user config needed (I2C address 0x27 hardcoded)
  - `aqara_fp2_accel.cpp/h`: AqaraFP2Accel class, polling loop, acceleration vector handling

**Root Level:**
- Purpose: Device-specific configs, visualization card, documentation, distribution metadata
- Key files:
  - `fp2-sala.yaml`: Production instance config (left_corner mounting, specific zone grids)
  - `example_config.yaml`: Comprehensive template showing all available options
  - `card.js`: Home Assistant Lovelace card for real-time grid visualization
  - `hacs.json`: HACS metadata (repository type, version)
  - `README.md`: Feature overview, installation, firmware list
  - `FLASHING.md`: esptool.py flashing steps, bootloader unlock

**`images/`:**
- Purpose: Documentation assets
- Contains: card_screenshot.png (UI rendering example)

## Key File Locations

**Entry Points:**
- `components/aqara_fp2/__init__.py` (line 208): CONFIG_SCHEMA — YAML validation entry point
- `components/aqara_fp2/__init__.py` (line 362): `async def to_code()` — code generation entry point
- `components/aqara_fp2/fp2_component.cpp`: FP2Component::loop() — UART polling entry point (called by ESPHome event loop)
- `card.js` (line 16): `class AqaraFP2Card` — HA card initialization

**Configuration:**
- `fp2-sala.yaml`: Device instance configuration (currently flashed/active)
- `example_config.yaml`: Template with all features; see lines 1-200 for uart, api, actions, zones
- `secrets.yaml.example`: Template for secrets (copy to `secrets.yaml` and fill in values)
- `.env` (not in repo): Runtime secrets for `esphome` CLI deployment

**Core Logic:**
- `components/aqara_fp2/__init__.py` (lines 114-175): `parse_ascii_grid()` function — grid ASCII↔binary conversion
- `components/aqara_fp2/fp2_component.cpp` (lines 14-28): CRC16-MODBUS calculation
- `components/aqara_fp2/fp2_component.cpp` (lines 30-100): `attr_id_to_string_()` — protocol reference table
- `components/aqara_fp2/fp2_component.h` (lines 109-136): OpCode/DataType enums — protocol definitions

**Testing:**
- `fp2-card-test.html`: Standalone test page for card.js (open in browser to debug card without HA)
- No unit tests detected; testing is via live device + HA integration

## Naming Conventions

**Files:**
- Python files: `snake_case.py` (e.g., `__init__.py`)
- C++ headers: `snake_case.h` (e.g., `fp2_component.h`)
- C++ implementation: `snake_case.cpp` (e.g., `fp2_component.cpp`)
- YAML configs: `kebab-case.yaml` (e.g., `fp2-sala.yaml`, `example_config.yaml`)
- JavaScript: `kebab-case.js` (e.g., `card.js`)
- Documentation: `UPPERCASE.md` (README, FLASHING, etc.)

**Python Variables & Functions:**
- Config keys (YAML): `CONF_SNAKE_CASE` (e.g., `CONF_MOUNTING_POSITION`, `CONF_RADAR_RESET_PIN`)
- Python functions: `snake_case()` (e.g., `parse_ascii_grid()`, `grid_to_hex_string()`)
- Enums: `CamelCase` or `SCREAMING_SNAKE_CASE` for values (e.g., `MOUNTING_POSITIONS = {"wall": 0x01, ...}`)

**C++ Naming:**
- Classes: `PascalCase` (e.g., `FP2Component`, `FP2Zone`, `AqaraFP2Accel`)
- Enums: `PascalCase` class, `SCREAMING_SNAKE_CASE` values (e.g., `enum class OpCode : uint8_t { RESPONSE = 0x01, WRITE = 0x02, ... }`)
- Methods: `snake_case()` (e.g., `handle_report_frame()`, `set_motion_timeout()`)
- Member variables: `snake_case_` (with trailing `_` for private members, e.g., `last_motion_millis_`)

**HA Entities:**
- Binary sensors: `binary_sensor.{device_name}_{feature}` (e.g., `binary_sensor.fp2_sala_global_presence`, `binary_sensor.fp2_sala_global_motion`)
- Sensors: `sensor.{device_name}_{feature}` (e.g., `sensor.fp2_sala_realtime_people_number`)
- Text sensors: `text_sensor.{device_name}_{feature}` (e.g., `text_sensor.fp2_sala_targets`)
- Switches: `switch.{device_name}_{feature}` (e.g., `switch.fp2_sala_report_targets`)
- Lights: `light.{device_name}_{feature}` (e.g., `light.fp2_sala_status_led`)

## Where to Add New Code

**New Radar Feature (firmware-derived SubID):**
- Python config key: Add `CONF_NEW_FEATURE = "new_feature"` to `components/aqara_fp2/__init__.py` (around line 55)
- Python schema: Add to CONFIG_SCHEMA in `__init__.py` with `cv.Optional()` + appropriate validator
- Python entity mapping: Add to SENSOR_MAP or ZONE_SENSOR_MAP in `__init__.py` (line 327+)
- C++ header: Add AttrId enum value to `fp2_component.h` (line 138+)
- C++ string mapping: Add case to `attr_id_to_string_()` in `fp2_component.cpp` (line 30+)
- C++ handling: Add SubID case in `handle_report_frame()` switch statement (location: fp2_component.cpp main parsing loop)
- YAML example: Update `example_config.yaml` with new option and entity config

**New Zone Type:**
- Add entry to `ZONE_TYPES` dict in `components/aqara_fp2/__init__.py` (line 101)
- Example: `"bathroom": 20,`

**New Mount Position:**
- Add entry to `MOUNTING_POSITIONS` dict in `components/aqara_fp2/__init__.py` (line 88)
- Update `card.js` grid rendering logic if mount affects visualization

**New ESPHome Action:**
- YAML config: Add to `api: actions:` section in `fp2-sala.yaml` or `example_config.yaml`
- Lambda: Call corresponding FP2Component method
- C++ method: Implement in `FP2Component` class (`fp2_component.h/cpp`)
- Example pattern (from `example_config.yaml` lines 37-40):
  ```yaml
  - action: fp2_force_detection_config
    then:
      - lambda: |-
          id(fp2).force_detection_config();
  ```

**New HA Card Feature:**
- Location: `card.js` (visualization logic)
- Canvas rendering: Modify `drawGrid()` or add new drawing method
- Entity polling: Update `updateCard()` to read additional sensor states
- Config options: Add to `setConfig()` validation + `this.config` fields
- Example (from card.js):
  - Add config key: `this.showNewFeature = config.show_new_feature !== false;`
  - Render in canvas: `if (this.showNewFeature) { /* draw */ }`

**Secrets & Secrets Management:**
- Copy `secrets.yaml.example` → `secrets.yaml` (not committed to git)
- Add secrets via `!secret` references in YAML:
  ```yaml
  wifi:
    ssid: !secret wifi_ssid
  api:
    encryption:
      key: !secret api_encryption_key
  ```
- Required secrets for ESPHome deployment: `wifi_ssid`, `wifi_password`, `api_encryption_key`, `ota_password`, etc.

**Device-Specific Configs:**
- Create new YAML file following `fp2-{location}.yaml` pattern (e.g., `fp2-kitchen.yaml`)
- Include unique zone grids, mounting position, optional features per room
- Must include `accel: fp2_accel` and `uart_id: uart_bus` (or define if different)

## Special Directories

**`components/`:**
- Purpose: ESPHome component source tree
- Generated: No; hand-written C++/Python
- Committed: Yes; core project deliverable

**`.planning/`:**
- Purpose: GSD (Gently Structured Development) planning documents
- Generated: Yes; created by `gsd-map-codebase` and `gsd-plan-phase` skills
- Committed: Yes; used for phase planning and code generation context

**`images/`:**
- Purpose: Documentation screenshots and diagrams
- Generated: No; manually captured/created
- Committed: Yes; referenced in README

## Implementation Guidelines

**Device Configuration (YAML) Flow:**
1. Create `fp2-{location}.yaml` in root
2. Define `esphome:`, `esp32:`, `wifi:`, `api:` sections (copy from `example_config.yaml`)
3. Add `uart:` section with GPIO pins
4. Add `i2c:` section with GPIO pins
5. Add `external_components:` referencing this repo + `[aqara_fp2, aqara_fp2_accel]`
6. Configure `aqara_fp2_accel:` with `id: fp2_accel`
7. Configure `aqara_fp2:` with required `accel: fp2_accel`, optional zones/grids/features
8. Add `api: actions:` for control (force_detection_config, set_work_mode, etc.)
9. Create `secrets.yaml` with wifi + api keys
10. Run `esphome run fp2-{location}.yaml` to build and flash

**Component Extension (New SubID):**
1. Research firmware dump to find SubID, data type, size
2. Add CONF_* key in Python `__init__.py`
3. Update CONFIG_SCHEMA with entity type (sensor, binary_sensor, text_sensor)
4. Add SENSOR_MAP entry pointing to entity factory + C++ setter method name
5. Implement C++ setter in FP2Component header (e.g., `void set_new_feature_sensor(Sensor* s)`)
6. Implement C++ publish in fp2_component.cpp parse loop
7. Test with `example_config.yaml` + esphome validate, then real device

---

*Structure analysis: 2026-07-12*
