# Coding Conventions

**Analysis Date:** 2026-07-12

## Naming Patterns

**Files:**
- Python: `__init__.py` (ESPHome component entry point)
- C++: `fp2_component.h`, `fp2_component.cpp` (PascalCase + underscore for separators)
- YAML configs: kebab-case with underscores allowed (e.g., `fp2-sala.yaml`)

**Python Functions:**
- snake_case for public functions: `parse_ascii_grid()`, `grid_to_hex_string()`
- Async functions prefixed with `async def`: `async def to_code(config)`
- Private functions: not observed in public API layer
- Configuration constants: UPPER_SNAKE_CASE: `CONF_MOUNTING_POSITION`, `CONF_FP2_ID`, `CONF_ZONES`

**C++ Methods:**
- PascalCase for public methods: `setup()`, `setup_pins()`, `force_detection_config()`
- snake_case_with_underscore_ for private methods: `perform_reset_()`, `attr_id_to_string_()`, `publish_radar_debug_()`
- Enum values: UPPER_SNAKE_CASE: `AttrId::MOTION_DETECT`, `OpCode::WRITE`

**YAML Entity Naming:**
- Human-readable quoted strings: `name: "Global Presence"`, `name: "Radar Software Version"`
- snake_case for IDs: `id: fp2_accel`, `id: uart_bus`
- Sensor/entity names use title case with spaces: `"Realtime People Number"`, `"Sleep In Out State"`

**Variables & Config Keys:**
- Python config keys: UPPER_SNAKE_CASE with CONF_ prefix: `CONF_RADAR_RESET_PIN`, `CONF_PRESENCE_SENSITIVITY`
- Mapped enum values: kebab-case in YAML: `mounting_position: left_corner`, `presence_sensitivity: medium`
- Enum constant dicts: UPPER_SNAKE_CASE with dict values: `MOUNTING_POSITIONS = {"wall": 0x01, "left_corner": 0x02}`

**Types & Classes:**
- C++ classes: PascalCase: `FP2Component`, `FP2Zone`, `FP2LocationSwitch`
- Python/ESPHome namespaces: snake_case: `aqara_fp2`, `aqara_fp2_accel`
- Type aliases: `using GridMap = std::array<uint8_t, 40>;`

## Code Style

**Formatting:**
- Python: 4-space indentation (ESPHome standard)
- C++: 2-space indentation (observed in fp2_component.cpp)
- Line length: varies; some lines exceed 100 characters
- YAML: 2-space indentation

**Linting:**
- No formal linter configuration found (`.eslintrc`, `.prettierrc` absent)
- Code follows ESPHome conventions implicitly through use of esphome.codegen and esphome.config_validation

**Code Blocks:**
- Schema definitions use method chaining: `.extend(ZONE_BASE_SCHEMA)`, `.extend(uart.UART_DEVICE_SCHEMA)`
- Configuration schemas are highly declarative using `cv.Schema()` dictionaries

## Import Organization

**Python Order (observed in `components/aqara_fp2/__init__.py`):**
1. Standard library: `import json`
2. ESPHome framework imports: `import esphome.codegen as cg`, `import esphome.config_validation as cv`
3. ESPHome component imports: `from esphome.components import ...`
4. ESPHome constants: `from esphome.const import (...)`
5. ESPHome core: `from esphome.core import ...`
6. Local/relative imports: `from ..aqara_fp2_accel import AqaraFP2Accel`

**Path Aliases:**
- No explicit alias configuration detected
- Relative imports use `..` for sibling component directories

**C++ Includes (observed in `fp2_component.cpp`):**
1. Local component headers: `#include "fp2_component.h"`
2. ESPHome component headers: `#include "esphome/components/..."`
3. ESPHome core headers: `#include "esphome/core/..."`
4. Standard library: `#include <vector>`, `#include <cmath>`
5. Third-party: `#include <ArduinoJson.h>`

## Error Handling

**Patterns:**
- Config validation errors use `cv.Invalid()`: raises exception with descriptive message during config parsing
  - Example: `raise cv.Invalid(f"Grid must have exactly 14 rows, got {len(lines)}")`
- String formatting with f-strings for error messages
- Enum range validation implicit through `cv.enum(SENSITIVITY_LEVELS)`
- Grid dimensions validated in `parse_ascii_grid()` before bit manipulation
- No exceptions thrown in C++ component code; errors logged via `ESP_LOGI()`, `ESP_LOGE()`

**Logging:**
- ESP-IDF logging macros: `ESP_LOGI(TAG, "format string", args)`, `ESP_LOGE()`
- TAG constant defined as: `static const char *const TAG = "aqara_fp2";`
- Detailed logging in protocol handling: `publish_radar_debug_()` logs events and hex payloads

## Comments

**When to Comment:**
- Reverse-engineering findings with firmware SubID references: `// Zone furniture/scene type (0x0152). From reverse-engineering PROTOCOL.md.`
- Protocol specifications: `// Each row is 2 bytes (16 bits) Big Endian.`
- Non-obvious bit manipulations: `// Bit 15 = Col 0 ... Bit 0 = Col 15` and `// Set bit at out_c`
- Configuration notes: `// 20 rows x 16 cols.` for grid structure
- Hardware layout: `// The stock board uses GPIO33/GPIO32 for the internal I2C bus.`
- State machine logic: `// Zone motion events (0x0115) are momentary. Hold the motion sensor ON...`

**Documentation Style:**
- Inline comments use `//` for both Python (used in adjacent YAML/protocol docs) and C++
- Multi-line explanations formatted as contiguous comment blocks
- References to firmware attributes use hex notation: `0x0152`, `0x0115`, `0x0158`
- Reverse-engineering sources documented inline without formal JSDoc

**JSDoc/TSDoc:**
- Not used in Python component code
- C++ header files use minimal inline documentation
- Type safety enforced through ESPHome type system (cg.class_, cg.new_Pvariable)

## Function Design

**Size:**
- Functions are generally focused and single-purpose
- `parse_ascii_grid()` handles grid parsing logic (20 lines of logic)
- `force_detection_config()` enqueues multiple commands but stays under 20 lines
- Protocol handling spread across methods for clarity

**Parameters:**
- Config validation functions accept parsed config dict: `async def to_code(config)`
- Conversion functions accept source type and return result: `def parse_ascii_grid(value) -> list`
- Sensor/entity creation uses keyword arguments: `binary_sensor_schema(device_class=..., filters=[...])`
- Command methods accept explicit parameters: `write_attr_uint8(uint16_t attr_id, uint8_t value)`

**Return Values:**
- Config converters return transformed value or raise `cv.Invalid()`
- Async code generators use `await cg.register_component()` pattern
- Helper functions return values directly: `grid_to_hex_string()` returns hex string

## Module Design

**Exports:**
- Python components expose: namespace objects (`aqara_fp2_ns`), classes (`FP2Component`, `FP2Zone`), config schema (`CONFIG_SCHEMA`), and async entry point (`to_code()`)
- Configuration constants (CONF_*) are module-level exports used in schema definitions
- Enum dictionaries exported at module level: `MOUNTING_POSITIONS`, `SENSITIVITY_LEVELS`, `ZONE_TYPES`

**Barrel Files:**
- No barrel files (index.ts-style re-exports) in this ESPHome project
- Each component has single `__init__.py` as entry point
- Imports reference sibling components: `from ..aqara_fp2_accel import AqaraFP2Accel`

**Configuration Constants:**
- All CONF_* constants defined in `__init__.py` for easy schema reference
- Enum mappings defined as module-level dicts for schema validators
- Reverse-engineering constants documented with SubID values (hex notation)

---

*Convention analysis: 2026-07-12*
