# Codebase Concerns

**Analysis Date:** 2026-07-12

## Tech Debt

**Reverse-Engineered Protocol Register Offsets:**
- Issue: All 66 register offsets (0x0XXX enum values) are reverse-engineered from packet captures with no official firmware documentation
- Files: `components/aqara_fp2/fp2_component.h` (lines 139-205)
- Impact: Register semantics may change between Aqara firmware versions; devices running different firmware versions may not respond to or interpret reads/writes correctly, causing silent failures or feature mismatches
- Fix approach: Establish baseline firmware version compatibility matrix; implement per-device firmware version detection and validation; consider feature negotiation during initialization

**Hardcoded Zone Type Mappings:**
- Issue: `ZONE_TYPES` dictionary (lines 101-111 in `__init__.py`) contains reverse-engineered mappings for furniture/scene detection (`tv: 2`, `green_plant: 10`, `shower: 23`, `stairs: 36`)
- Files: `components/aqara_fp2/__init__.py`
- Impact: Uncertain if these values are complete or accurate; unknown zone types may be silently ignored or cause unexpected radar behavior
- Fix approach: Document source of each zone type value; add fallback for unknown types; test against multiple Aqara firmware versions

**OTA Flag Register (0x0127) Semantics Unclear:**
- Issue: `OTA_SET_FLAG = 0x0127` is reverse-engineered; actual firmware update mechanism and data format unknown
- Files: `components/aqara_fp2/fp2_component.h` (line 152)
- Impact: Risk of firmware corruption or device bricking if OTA sequence is incorrect; no validation of OTA payload format
- Fix approach: Never expose OTA_SET_FLAG for direct user writes; implement strict OTA validation against Aqara's original firmware update tool behavior

**ZONE_ACTIVATION_LIST (0x0202) Purpose Unknown:**
- Issue: 32-byte auxiliary configuration register with undocumented semantic meaning
- Files: `components/aqara_fp2/fp2_component.h` (line 188)
- Impact: Misconfigured auxiliary settings could degrade detection performance or cause unstable behavior
- Fix approach: Reverse-engineer through empirical testing; document byte-by-byte structure; provide sensible defaults

## Known Bugs

**Grid Sensor State Initialization Race:**
- Symptoms: Grid sensors (edge_label_grid, entry_exit_grid, interference_grid) may not publish initial values if sensors are registered after component setup
- Files: `components/aqara_fp2/fp2_component.h` (lines 282-310), `fp2_component.cpp` (lines 495-515)
- Trigger: Grid values are only published when grids are received from radar; if sensors are added after initial grid read, initial state is lost
- Workaround: Ensure all text sensors are declared before `aqara_fp2:` component initialization in YAML

**Header Checksum Validation Weakness:**
- Symptoms: Malformed frames with incorrect header checksums trigger state machine reset but may allow occasionally corrupted packets through if bit errors align
- Files: `fp2_component.cpp` (lines 724-738)
- Trigger: Frame corruption during UART transmission at 890000 baud (high speed)
- Workaround: Rely on CRC16 at end of frame to catch true corruption; monitor logs for frequent "Header Checksum Fail" warnings
- Fix approach: Add CRC validation BEFORE processing payload; implement frame retry on header checksum failure

**Motion Timeout Debounce Not Reset on Absence:**
- Symptoms: Zone motion sensor may remain ON if no "Exit" event (0x0115 with bit 0x04) is received from radar
- Files: `fp2_component.cpp` (lines 74-85), `fp2_component.h` (lines 64-79)
- Trigger: Radar stops reporting motion but never sends explicit "Exit" event (firmware quirk or detection logic gap)
- Workaround: Set motion_timeout to reasonable value (default 5s); manually trigger zone reset if needed
- Fix approach: Implement watchdog that forces motion OFF after extended absence of ANY motion event; track last radar frame timestamp

## Security Considerations

**UART Configuration at 890000 Baud:**
- Risk: Non-standard baud rate (890000 = 890.0 kbps) is unusual; could make traffic analysis difficult or indicate vendor lock-in attempt
- Files: `fp2-sala.yaml` (line 81)
- Current mitigation: None; UART bus is local to device
- Recommendations: Verify this baud rate matches Aqara FP2 hardware spec; test at lower rates (115200, 460800) for debugging without hardware modification

**Web Server Exposed on Device IP:**
- Risk: ESPHome web_server (version 2) serves JSON API and HTML dashboard without mandatory authentication; exposed to network
- Files: `fp2-sala.yaml` (lines 28-30)
- Current mitigation: None; captive portal does not protect web_server
- Recommendations: Add `auth:` block with password to `web_server:` config; restrict via firewall to trusted IPs only; consider disabling in production and use ESPHome API instead

**API Encryption Key Exposure:**
- Risk: ESPHome API encryption key stored in `!secret` (good), but template declares it plainly (acceptable for checked-in configs)
- Files: `fp2-sala.yaml` (line 34)
- Current mitigation: Key is loaded from secrets.yaml (not in repo)
- Recommendations: Ensure secrets.yaml is in .gitignore; rotate key if device is shared or commissioned code is distributed

**Location Tracking Data Exposure:**
- Risk: `target_tracking` sensor publishes real-time target locations as text; could reveal room occupancy patterns to MQTT subscribers
- Files: `fp2_component.cpp` (lines 1000+), `__init__.py` (lines 234-235)
- Current mitigation: None; data flows to Home Assistant
- Recommendations: Mark as diagnostic entity; restrict MQTT publish via HA automation; consider disabling in privacy-sensitive environments

## Performance Bottlenecks

**Memory Allocation Patterns on ESP32-SOLO1:**
- Problem: Component allocates 13+ std::vector instances for grid data (40 bytes each), zone structs, and command queues; SOLO1 variant has only 320KB SRAM
- Files: `fp2_component.cpp` (lines 308-449, 598-621), `fp2_component.h` (lines 27-28, 474-475, 510)
- Cause: Vectors are dynamically allocated; no pre-allocation or pooling; each zone creates new FP2Zone struct
- Improvement path: Use fixed-size arrays instead of std::vector for grid data; pre-allocate command queue; consider disabling unused features (sleep mode, people counting) if memory becomes critical

**Web Server Memory Cost:**
- Problem: ESPHome web_server component consumes 40-60KB heap depending on page complexity; fp2-sala enables both web_server and full FP2 feature set
- Files: `fp2-sala.yaml` (lines 28-30)
- Cause: Web server stores HTML, CSS, JavaScript, and JSON state for all sensors in RAM
- Improvement path: Monitor heap via `ESP_LOG(TAG, "Free heap: %u", ESP.getFreeHeap())` after setup; if <100KB available, disable web_server or offload dashboard to separate HA frontend; consider sram1_as_iram (already enabled in config line 14, good)

**Command Queue Unbounded Growth:**
- Problem: Command queue (`std::deque<FP2Command>`) has no size limit; rapid sensor updates could queue hundreds of commands
- Files: `fp2_component.h` (line 510), `fp2_component.cpp` (lines 542-592)
- Cause: `enqueue_command_*()` methods add without checking queue length
- Improvement path: Implement max queue depth (suggest 32); drop oldest non-critical command if exceeded; log warning when threshold hit

**UART RX Buffer Overrun at 890000 Baud:**
- Problem: High baud rate (890kbps) combined with large payloads (up to 4096 bytes per line 730 sanity check) could overflow ESP-IDF UART ringbuffer if loop() is blocked by other tasks
- Files: `fp2_component.cpp` (line 730)
- Cause: Default UART buffer is ~512 bytes; at 890kbps, 512 bytes arrives in <4.6ms; if loop() is blocked >4.6ms, frames are lost
- Improvement path: Increase UART buffer in sdkconfig (CONFIG_UART_HW_FLOWCTRL or CONFIG_UART_RX_FIFO_FULL_THRS); implement hardware flow control if FP2 hardware supports it; monitor for "CRC Fail" spikes

## Fragile Areas

**Protocol State Machine in handle_incoming_byte_():**
- Files: `fp2_component.cpp` (lines 685-787)
- Why fragile: 11-state machine (SYNC → VER_H → VER_L → SEQ → OPCODE → LEN_H → LEN_L → H_CHECK → PAYLOAD → CRC_L → CRC_H) synchronizes on single 0x55 byte; bit errors in sync byte can cause extended desynchronization; no timeout to force resync
- Safe modification: Any change to state transitions requires exhaustive testing with corrupted frame injection; add state timeout (force SYNC after 100ms in any state); implement frame length limits with assert
- Test coverage: No unit tests for frame decoder; recommend fuzzing with random byte sequences

**Zone Motion Event Processing:**
- Files: `fp2_component.cpp` (lines 868-889)
- Why fragile: Motion event bitmask (0x0115 payload[4]) decoding assumes specific bit layout (0x01=Enter, 0x02=Move, 0x04=Exit, 0x08=L/R, 0x10=Interference); undocumented if firmware can send compound bits
- Safe modification: Add exhaustive switch on event_type; log unrecognized bits; never assume zero bits mean no motion
- Test coverage: No test cases for all motion event types

**Grid Data Parsing (parse_ascii_grid in __init__.py):**
- Files: `__init__.py` (lines 114-155)
- Why fragile: Assumes 14x14 input grid maps to 20x16 output with magic offset (row offset=0, col offset=2); no comment explaining why these offsets are correct
- Safe modification: Add detailed ASCII diagram showing row/column mapping; validate output grid has correct active cells; add unit tests with known-good grid examples
- Test coverage: No validation that output grid is sensible (all zeros or all ones would pass)

## Scaling Limits

**Zone Count Hard Limit:**
- Current capacity: Code supports arbitrary zone count via `std::vector<FP2Zone*>` loop
- Limit: ESP32 RAM (~320KB SRAM) limits to ~8-10 practical zones given other component overhead; each zone struct + 40-byte grid + sensors ~10-15KB
- Scaling path: For >10 zones, consider: (a) disable non-critical features (sleep, thermodynamic chart), (b) move to ESP32-S3 (8MB PSRAM), (c) implement zone persistence (store zones in flash, load on demand)

**Target Tracking Object Limit:**
- Current capacity: Location tracking data is buffered in `target_tracking_sensor_` text sensor (string state)
- Limit: Text sensor state is capped at ~256 chars in ESPHome; target tracking JSON can grow to >500 chars with 10+ targets
- Scaling path: Split targets across multiple sensors or implement pagination; compress JSON format; consider external storage via API

**Command Queue Throughput:**
- Current capacity: Commands are processed serially with 500ms ACK timeout + 500ms READ timeout
- Limit: Max ~1 command/sec; rapid configuration changes (e.g., enabling all sleep features) may queue 20+ commands = 20+ second wait
- Scaling path: Implement command batching/coalescing; reduce timeout to 250ms if UART reliability allows; implement priority queue (config > diagnostics)

## Dependencies at Risk

**ArduinoJson Library:**
- Risk: Dependency declared in `#include <ArduinoJson.h>` (fp2_component.h line 13); version not pinned; major version upgrades could break API
- Impact: JSON serialization for map config could silently fail or allocate excessive memory
- Migration plan: Pin to known-good version in platformio.ini; test map_config serialization after updates; consider switching to std::string + manual JSON for simple payloads

**ESP-IDF UART Component:**
- Risk: UART bus is shared with FP2 sensor at 890000 baud; if another component tries to use same UART or change baud, collision occurs
- Impact: Cannot debug via serial monitor while FP2 is communicating
- Migration plan: Use separate UART pins for debug (USB-JTAG on GPIO 41/40 on newer ESP32-S3); document UART pinout in README

## Missing Critical Features

**OTA Firmware Update Capability:**
- Problem: OTA_SET_FLAG (0x0127) register exists but is not implemented; ESPHome OTA is separate from device firmware OTA
- Blocks: Cannot update Aqara FP2 firmware from ESPHome; must use Zigbee gateway or Aqara Home app
- Priority: Medium (firmware updates are infrequent; not blocking core functionality)

**Configuration Persistence:**
- Problem: All settings (grids, zone types, sensitivity) are written to radar on each boot but not stored locally; if radar loses power between boots, settings may be lost
- Blocks: No recovery mechanism if radar settings are corrupted
- Priority: Low (radar maintains internal state across restarts; ESPHome re-applies config on boot)

**UART Flow Control:**
- Problem: No CTS/RTS or software flow control implemented; high baud rate (890kbps) at long cable lengths could cause frame loss
- Blocks: Unreliable operation on noisy or long UART runs
- Priority: Medium (only affects edge cases with poor cable shielding)

## Test Coverage Gaps

**Frame Decoder State Machine:**
- What's not tested: Bit-error injection; out-of-order frames; sync byte appearing mid-payload; length field = 0xFFFF; CRC mismatches
- Files: `fp2_component.cpp` (lines 685-787)
- Risk: Silent frame drops or state machine hangs possible but undetected
- Priority: High (decoder is core to protocol reliability)

**Zone Motion Debounce Logic:**
- What's not tested: Multiple motion events in <5s; motion timeout = 0; rapid zone enable/disable
- Files: `fp2_component.h` (lines 74-85)
- Risk: Motion sensor may flicker or get stuck ON/OFF
- Priority: Medium (affects user experience but not safety)

**Grid Data Encoding/Decoding:**
- What's not tested: Round-trip of grid data (parse_ascii_grid → send → receive → display); edge rows/columns; all-zero and all-one grids
- Files: `__init__.py` (lines 114-155), `fp2_component.cpp` (lines 391-??)
- Risk: Grids sent to radar may not match received grids; silent masking of detection zones
- Priority: High (affects core detection feature)

**Register Write Validation:**
- What's not tested: Writes to read-only registers; out-of-range values; type mismatches (writing string to uint16 register)
- Files: `fp2_component.h` (lines 378-381), `fp2_component.cpp` (lines 546-551)
- Risk: Silent failures or radar misbehavior if invalid data is sent
- Priority: Medium (safeguards exist in Python config validation, but C++ layer has none)

**Sleep Mode Configuration:**
- What's not tested: configure_sleep_mode() with extreme dimensions; sleep mode + fall detection interaction; sleep state transitions
- Files: `fp2_component.h` (line 382)
- Risk: Sleep features may not activate or may interfere with normal presence detection
- Priority: Low (sleep features are optional; most deployments use normal mode)

---

*Concerns audit: 2026-07-12*
