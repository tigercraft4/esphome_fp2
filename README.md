# Aqara FP2 ESPHome & Home Assistant Integration

> **⚠️ WARNING: EXPERIMENTAL SOFTWARE ⚠️**
> This project is in early development and **not well tested**. Use at your own risk!
> Features may be incomplete, unstable, or change without notice.
> Please report issues and contribute improvements if you encounter problems.

Custom ESPHome components and Home Assistant visualization card for the Aqara FP2 Presence Sensor.

![Card screenshot](images/card_screenshot.png)

See [FLASHING.md](FLASHING.md) for flashing instructions.

## Overview

This project provides two main components for working with the Aqara FP2 mmWave presence sensor:

1. **ESPHome Components** - Custom components for flashing and controlling the FP2 hardware directly with ESPHome
2. **Home Assistant Card** - Interactive visualization card for viewing real-time presence data, zones, and radar tracking

## Features

- Direct UART communication with FP2 radar sensor
- Real-time target tracking with position and velocity
- Customizable detection zones with individual sensitivity settings
- Interference grid and entry/exit zone configuration
- Visual representation of sensor coverage area
- Zone occupancy and motion detection
- Interactive web card with full/zoomed display modes
- Firmware-derived report toggles and diagnostics for people counting, walking distance, sleep state, target posture, and radar debug logs
- Shared ESPHome I2C support for the board accelerometer and OPT3001 illuminance sensor
- RGB status LED control on the stock FP2 LED channels

---

## Part 1: ESPHome Components

The ESPHome components allow you to flash custom firmware to the Aqara FP2 and integrate it directly with Home Assistant via the ESPHome API.

### Components

- **`aqara_fp2`** - Main component for FP2 radar sensor control and data collection
- **`aqara_fp2_accel`** - I2C accelerometer interface for mounting position detection

### Installation

For remote installation (once published):

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Ripthulhu/esphome_fp2
      ref: main
    components: [aqara_fp2, aqara_fp2_accel]
```

### Configuration Example

See [example_config.yaml](example_config.yaml) for a complete working configuration.

### FP2 Board Hardware

The stock board uses GPIO33/GPIO32 for the internal I2C bus. The accelerometer is at `0x27`; the OPT3001 illuminance sensor is at `0x44`. The status LED is wired as an inverted RGB LED with red on GPIO14, green on GPIO26, and blue on GPIO27.

### Grid Configuration

The FP2 uses a 14×14 grid to map the detection area:
- `.` = Active detection cell
- `X` = Zone coverage
- Grids available: `interference_grid`, `exit_grid`, `edge_label_grid`

### Firmware-Derived Radar Options

The ESPHome component exposes several SubIDs found in the stock firmware dump. Defaults preserve the current component behavior for the already-used report streams.

| Option | Radar SubID | Default | Notes |
|--------|-------------|---------|-------|
| `people_counting_report_enable` | `0x0158` | `true` | Enables people-counting reports. |
| `people_number_enable` | `0x0162` | `true` | Enables current/ontime people-number reports. |
| `target_type_enable` | `0x0163` | `false` | Stock app labels this as AI Person Detection; lab testing showed `true` can suppress target streaming. |
| `dwell_time_enable` | `0x0172` | `false` | Enables dwell-time reporting path. |
| `walking_distance_enable` | `0x0173` | `false` | Enables accumulated walking-distance reports. |
| `thermodynamic_chart_enable` | `0x0138` | `true` | Enables thermodynamic chart reports; raw BLOB remains in `radar_debug`. |
| `sleep_report_enable` | `0x0156` | unset | Optional; enables sleep reports when configured. |
| `posture_report_enable` | `0x0157` | unset | Optional; enables target-posture reports when configured. |
| `fall_detection` | `0x0121` | unset | Optional fall-detection enable. |
| `fall_detection_sensitivity` | `0x0123` | unset | `low`, `medium`, or `high`. |

Optional entities decoded from simple firmware-confirmed report wrappers:

| Entity key | Radar SubID | Type |
|------------|-------------|------|
| `realtime_people_number` | `0x0164` | `UINT32` sensor |
| `ontime_people_number` | `0x0165` | `UINT32` sensor |
| `realtime_people_counting` | `0x0166` | `UINT32` sensor |
| `people_counting` | `0x0155` | text sensor (`id=<n> ontime=<n> realtime=<n>`) |
| `walking_distance` | `0x0174` | `UINT32` sensor |
| `sleep_data` | `0x0159` | diagnostic text sensor with raw 12-byte blob plus little/big-endian word hypotheses |
| `sleep_presence` | `0x0167` | binary sensor |
| `sleep_inout_state` | `0x0171` | binary sensor |
| `sleep_state` | `0x0161` | text sensor |
| `sleep_event` | `0x0176` | text sensor |
| `target_posture` | `0x0154` | text sensor |
| `radar_debug` | `0x0201`, unhandled reports, and command telemetry | diagnostic text sensor |

Diagnostic actions exposed in the example config:

| Action | Purpose |
|--------|---------|
| `fp2_force_detection_config` | Replays the known-good presence/live-map setup (`work_mode=3`, reporting enabled, AI target filter from config). |
| `fp2_configure_sleep_mode` | Replays the confirmed app-like sleep setup. Parameters are `width`, `length`, and `mount_position`; the stock capture value `120 x 180`, mount `1`, writes `sleep_zone_size=0x007800B4`. |
| `fp2_write_attr_uint8` / `fp2_write_attr_uint16` / `fp2_write_attr_uint32` / `fp2_write_attr_bool` | Raw attribute write probes for reverse engineering. |
| `fp2_set_work_mode` | Writes raw `work_mode`; confirmed `3=presence`, `9=sleep` on the live sensor. |
| `fp2_calibrate_empty_room` | Sends the current empty-room calibration candidate `reset_absent_status` (`0x0113=TRUE`). |

`radar_debug` now also publishes command TX/ACK/retry/timeout events so Home Assistant can show whether a probe write reached the radar. For complete ordering, use native ESPHome logs; HA history can coalesce rapid text-sensor updates.

### Flashing Instructions

See [FLASHING.md](FLASHING.md) for flashing instructions.

---

## Part 2: Home Assistant Card

An interactive visualization card for viewing FP2 sensor data in Home Assistant.

### Installation via HACS

This is the primary and recommended installation method.

1. **Add Custom Repository** (if not yet published to HACS default):
   - Open HACS in Home Assistant
   - Go to "Frontend"
   - Click the three dots (⋮) in the top right
   - Select "Custom repositories"
   - Add repository URL: `https://github.com/Ripthulhu/esphome_fp2`
   - Category: `Dashboard`

2. **Install the Card**:
   - Search for "FP2" or "Aqara FP2 Presence Sensor Card"
   - Click "Download"
   - Restart Home Assistant

3. **Add to Dashboard**:
   ```yaml
   type: custom:aqara-fp2-card
   entity_prefix: sensor.fp2_living_room
   title: Living Room FP2
   display_mode: full  # Options: full, zoomed
   show_grid: true
   show_sensor_position: true
   show_zone_labels: true
   ```

### Manual Card Installation

Copy [card.js](card.js) to `/config/www/community/aqara-fp2-card/card.js` and add this Lovelace resource:

```yaml
url: /local/community/aqara-fp2-card/card.js
type: module
```

### Card Features

- **Real-time target tracking**: Shows detected people/objects with position and velocity vectors
- **Zone visualization**: Color-coded zones showing occupancy status
- **Grid overlays**: Edge labels, interference sources, entry/exit zones
- **Display modes**:
  - Full view (entire 14×14 grid)
  - Zoomed view (active detection area only)
- **Interactive controls**: Click to toggle view modes
- **Sensor position marker**: Shows FP2 mounting location

### Card Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `entity_prefix` | string | **required** | Entity prefix (e.g., `sensor.fp2`) |
| `title` | string | "Aqara FP2 Presence Sensor" | Card title |
| `display_mode` | string | `full` | Display mode: `full` or `zoomed` |
| `show_grid` | boolean | `true` | Show grid lines |
| `show_sensor_position` | boolean | `true` | Show sensor position marker |
| `show_zone_labels` | boolean | `true` | Show zone names |
| `mounting_position` | string | from entity | Override mounting position |
| `report_switch_entity` | string | derived from `entity_prefix` | Override the live-view switch entity |
| `map_config_service` | string | derived from `entity_prefix` | Override the ESPHome map-config service name |
| `targets_entity` | string | derived from `entity_prefix` | Override the target-tracking text sensor entity |

### Zone & Grid Editor

![Zone editor screenshot](images/card_editor_screenshot.png)

Paint zones and grids directly on the live radar view and export them as ready-to-paste YAML — no more hand-editing 14×14 ASCII grids.

1. **Enter edit mode** - click **Edit** in the card header (pencil icon) to switch the canvas to the full 14×14 grid and reveal the editor toolbar. Click **Done** to exit edit mode.
2. **Select a layer** - use the **Layer** dropdown to choose what you're editing: `Interference Grid`, `Exit Grid`, `Edge Grid`, or any zone (device zones and locally-added zones both appear in the list).
3. **Paint / erase** - with **Paint** active, click and drag on the grid to fill cells on the selected layer. Hold Shift or right-click while dragging to erase, no matter which mode is currently selected. **Clear Layer** wipes the selected layer's grid in one step.
4. **Add / remove zones** - click **+ Add Zone** to create a new local-only zone; it appears in the Layer dropdown as `Zone N`. Select a locally-added zone and click **Remove Zone** to delete it — **Remove Zone** is only shown for locally-added zones, not for zones read from the device.
5. **Set per-zone controls** - with a zone selected in the Layer dropdown, the **Zone** panel below the toolbar lets you set its label (locally-added zones only; device zone labels are read-only), **Type** (`zone_type`), **Sensitivity** (`presence_sensitivity`: Low/Medium/High), and an optional **Motion timeout** checkbox plus a seconds field.
6. **Set the Global Zone sensitivity (optional)** - the standalone **Global Zone** dropdown in the toolbar sets a card-wide `global_zone.presence_sensitivity`. Leave it on `-- not set --` to omit the `global_zone:` block from the export entirely.
7. **Export YAML** - click **Export YAML** to build a `zones:`/global-grid YAML block from your current edits. If the grids look suspicious (e.g. empty), you'll see a confirmation warning first. The result is copied to your clipboard when possible, and is always shown in a read-only text box below the toolbar as a fallback — paste it directly under your existing `aqara_fp2:` block (no extra wrapper key needed).
8. **Import from Device** - click **Import from Device** to re-fetch the sensor's current configuration and merge it into the editor. After you confirm, this overwrites the global grids and every existing device zone's grid/sensitivity with what the device currently reports, while keeping any locally-added zones untouched. Because the device can't report `zone_type`, `motion_timeout`, or the Global Zone sensitivity, those are reset to their defaults on every Import — re-set them before exporting again if you need them.

---

## Project Structure

```
esphome_fp2/
├── components/
│   ├── aqara_fp2/           # Main FP2 component
│   │   ├── __init__.py
│   │   ├── fp2_component.cpp
│   │   ├── fp2_component.h
│   │   ├── binary_sensor.py
│   │   └── text_sensor.py
│   └── aqara_fp2_accel/     # Accelerometer component
│       ├── __init__.py
│       ├── aqara_fp2_accel.cpp
│       └── aqara_fp2_accel.h
├── card.js                   # Home Assistant visualization card
├── hacs.json                 # HACS integration metadata
├── example_config.yaml       # Example ESPHome configuration
└── README.md                 # This file
```

---

## Requirements

### ESPHome Components
- ESPHome 2024.x or later
- ESP32 (ESP32-SOLO-1 in stock FP2 hardware)
- ESP-IDF framework

### Home Assistant Card
- Home Assistant 2024.x or later
- HACS (recommended) or manual installation
- ESPHome integration configured with FP2 device

---

## Contributing

This is an experimental project and contributions are welcome!

Documentation around usage, improvements, or general feedback is welcome. I have made this mostly for myself, so I haven't put a massive amount of effort into making it easy to use.

See https://github.com/hansihe/AqaraPresenceSensorFP2ReverseEngineering for more technical details.

---

## Disclaimer

This project is not affiliated with or endorsed by Aqara. Use at your own risk. The authors are not responsible for any damage to your hardware or any issues arising from the use of this software.
