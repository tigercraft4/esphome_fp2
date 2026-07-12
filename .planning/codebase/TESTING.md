# Testing Patterns

**Analysis Date:** 2026-07-12

## Test Framework

**Runner:**
- ESPHome compile-and-validate system (no pytest/unittest framework found)
- Config validation through `esphome.config_validation` module
- Manual on-device verification for protocol correctness

**Assertion Library:**
- Not applicable — testing occurs via schema validation and runtime behavior

**Run Commands:**
```bash
# Validate config (check YAML syntax and schema conformance)
esphome config <config_file>.yaml

# Compile firmware for target device
esphome compile <config_file>.yaml

# Flash to device (requires board connected)
esphome run <config_file>.yaml
```

## Test File Organization

**Location:**
- Test configs: at repository root (e.g., `example_config.yaml`, `fp2-sala.yaml`)
- Component code: `components/aqara_fp2/__init__.py` and `.cpp/.h` files
- No dedicated test/ directory found

**Naming:**
- Configuration examples follow pattern: `<device-name>.yaml` (e.g., `fp2-sala.yaml`)
- Example template: `example_config.yaml` (comprehensive reference with all available features)

**Structure:**
```
esphome_fp2/
├── components/
│   ├── aqara_fp2/          # Main FP2 component (UART + sensor logic)
│   └── aqara_fp2_accel/    # I2C accelerometer component
├── example_config.yaml     # Full reference configuration
└── fp2-sala.yaml          # Device-specific production config
```

## Test Structure

**Schema Validation Pattern (Python):**
```python
# Schema definition with validators and defaults
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FP2Component),
            cv.Required("accel"): cv.use_id(AqaraFP2Accel),
            cv.Optional(CONF_RADAR_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_MOUNTING_POSITION, default="left_corner"): cv.enum(MOUNTING_POSITIONS),
            cv.Optional(CONF_FALL_DETECTION): cv.boolean,
            # ... more validators ...
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)
```

**Patterns:**
- Config validation: `cv.Schema()` with required/optional fields
- Type coercion: `cv.enum()`, `cv.boolean`, `cv.positive_time_period_milliseconds`, `pins.gpio_output_pin_schema`
- Default values: `cv.Optional(key, default=value)`
- Custom validators: `parse_ascii_grid()` function validates grid format and converts to byte array
- Dependency injection: `cv.use_id()` for component references (e.g., `cv.use_id(AqaraFP2Accel)`)

**Error Handling:**
```python
def parse_ascii_grid(value):
    """Parses 14x14 ASCII grid; raises cv.Invalid on format mismatch."""
    lines = value.strip().splitlines()
    lines = [li.strip() for li in lines if li.strip()]

    if len(lines) != 14:
        raise cv.Invalid(f"Grid must have exactly 14 rows, got {len(lines)}")

    for i, line in enumerate(lines):
        clean_line = line.replace(" ", "")
        if len(clean_line) != 14:
            raise cv.Invalid(
                f"Row {i + 1} must have 14 characters (excluding spaces), got {len(clean_line)}: '{clean_line}'"
            )
    # ... conversion logic ...
```

## Mocking

**Framework:**
- No mocking framework detected (MockMock, unittest.mock absent)
- Testing is integration-level: config validation + on-device hardware verification

**Approach:**
- Config validation tests input schemas and transformations
- Protocol behavior validated by sending commands to real/emulated hardware via UART
- No unit test isolation layer

**Fixtures:**
- Enum fixtures defined as module-level dicts:
  ```python
  MOUNTING_POSITIONS = {
      "wall": 0x01,
      "left_corner": 0x02,
      "right_corner": 0x03,
  }
  SENSITIVITY_LEVELS = {
      "low": 1,
      "medium": 2,
      "high": 3,
  }
  ZONE_TYPES = {
      "none": 0,
      "tv": 2,
      "green_plant": 10,
      # ...
  }
  ```
- Grid examples embedded in config files (ASCII art format):
  ```yaml
  interference_grid: |-
    . . . . . . . . . . . . . .
    . . . . . . . . . . . . . .
    # ... 14 rows of . or x characters ...
  ```

## Coverage

**Requirements:**
- No formal coverage target enforced
- Testing occurs at schema validation (config parsing) + on-device runtime behavior

**Test Data:**
- Example configs serve as integration test cases:
  - `example_config.yaml`: Comprehensive feature coverage including all optional sensors/actions
  - `fp2-sala.yaml`: Real-world production device config with specific grid layouts

## Test Types

**Configuration Validation:**
- Schema validation tests that configs conform to expected structure
- Type coercion: enum values mapped to numeric codes, time periods converted to milliseconds
- Grid parsing: validates 14×14 ASCII format and converts to 40-byte protocol buffer
- Dependency resolution: ensures required components (e.g., `accel: fp2_accel`) are referenced
- Device class assignment: validates binary_sensor/sensor entity categories

**Example Test Scenario (manual verification):**
1. Write device config (e.g., `fp2-sala.yaml`) with custom grid
2. Run `esphome compile fp2-sala.yaml` to validate schema and generate C++ code
3. Flash firmware to ESP32 board with FP2 sensor
4. Monitor UART traffic to verify:
   - Grid configuration transmitted to radar correctly
   - Zone motion/presence events parsed from radar responses
   - Sensitivity levels applied per zone
   - Location reporting toggle works

**On-Device Protocol Testing:**
- Actions in config trigger radar communication:
  ```yaml
  actions:
    - action: get_map_config
      supports_response: only
      then:
        - api.respond:
            data: !lambda |-
              id(fp2).json_get_map_data(root);
  ```
- Lambda expressions validate JSON response payload structure
- Text sensors (`radar_debug`) capture protocol events for inspection:
  - Command transmission logs: `"TX read PRESENCE_DETECT_SENSITIVITY 0x0117 len=..."`
  - ACK receipts: `"ACK read 0x0117"`
  - Timeout detection: `"TIMEOUT read 0x0117"`

## Reverse-Engineering Validation

**Documentation Pattern:**
- Attribute mappings documented with firmware SubID references:
  ```python
  # New Options
  CONF_FALL_DETECTION = "fall_detection"
  # Maps to firmware SubID 0x0121
  ```
- Protocol specs embedded as comments:
  ```cpp
  // Zone motion events (0x0115) are momentary. Hold the motion sensor ON and
  // only release it after motion_timeout_ms with no new movement event (debounce).
  ```
- Grid protocol documented:
  ```python
  # Protocol Grid: 20 rows x 16 cols.
  # Active Area: Centered 14x14 (Rows 3-16, Cols 1-14).
  ```

**Validation Method:**
- Firmware dump analysis to extract attribute codes and response formats
- Lab testing of hypothesis functions: "enabled true; can suppress target streaming"
- Cross-reference with stock app behavior to confirm settings match

## CI/CD

**Build System:**
- ESPHome CLI handles compilation (`esphome compile`)
- No GitHub Actions or external CI pipeline found in repo
- Python version constraint: `requires-python >= 3.11` (pyproject.toml)
- Dependency pinning: `esphome >= 2025.12.4`

**Validation Checklist (pre-flash):**
1. YAML syntax valid (`esphome config`)
2. All referenced components installed
3. Pin assignments don't conflict
4. Schema validation passes (types, defaults, dependencies)
5. C++ compilation succeeds (`esphome compile`)
6. Firmware size fits ESP32-SOLO1 flash

---

*Testing analysis: 2026-07-12*
