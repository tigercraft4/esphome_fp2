# Aqara FP2 ESPHome — GUI Zone Editor

## What This Is

An ESPHome integration for the Aqara FP2 mmWave presence sensor: reverse-engineered C++ external components that speak the FP2 UART radar protocol, plus a Home Assistant Lovelace card (`card.js`) that renders the 14×14 detection grid and live target tracking. This milestone adds a **graphical zone editor to the card** so zones and the interference/exit/edge maps can be *drawn* on a live radar view and *exported to YAML*, replacing error-prone hand-editing of 14×14 ASCII grids.

## Core Value

A user can draw a zone by painting cells on the grid and get a valid `zones:` YAML block that flashes without `parse_ascii_grid` errors — no hand-editing of ASCII grids.

## Requirements

### Validated

<!-- Inferred from existing codebase (map: .planning/codebase/). -->

- ✓ ESPHome C++ components decode the FP2 UART radar protocol (frame state machine, CRC16) — existing
- ✓ 14×14 grid model with `parse_ascii_grid` validation (ASCII → 40-byte blob) — existing
- ✓ Detection zones with `zone_type` (0x0152), presence + motion binary sensors, motion debounce — existing
- ✓ Lovelace `card.js` renders 14×14 grid, draws zones, decodes live `target_tracking` base64 stream natively in HA (no MQTT) — existing
- ✓ Component exposes current config as JSON via HA action `get_map_config` (`json_get_map_data`) — existing
- ✓ Local diagnostics web server on device — existing

### Active

<!-- This milestone. Hypotheses until shipped and validated. -->

- [ ] Card has an **Edit** toggle that enters/exits editor mode
- [ ] Layer selector picks what is being painted: each zone, `interference_grid`, `exit_grid`, `edge_grid`
- [ ] Paint/erase cells by click+drag and touch on the 14×14 grid
- [ ] Per-zone controls: `zone_type` dropdown, `presence_sensitivity` (low/medium/high), optional `motion_timeout`
- [ ] Live target overlay stays visible during editing; column mapping honors `left_right_reverse`
- [ ] Grid ↔ ASCII (14×14) serializer that matches `parse_ascii_grid` exactly
- [ ] **Export YAML** button generates config blocks (only-set optional keys) and copies to clipboard
- [ ] **Import** current device config via `get_map_config` to pre-load the layout for editing (round-trips back to YAML)
- [ ] README card section updated + screenshot added to `images/`
- [ ] **Spike:** assess feasibility of runtime "Save to Sensor" (write ZONE_MAP 0x0114 via `enqueue_command_blob2_`, transport = native HA action) — investigation only, no full feature

### Out of Scope

- MQTT on the FP2 — card already talks to the device over the native ESPHome API inside HA; a broker adds dependency/config/security surface with no benefit (even for the future runtime write path)
- Full runtime "Save to Sensor" feature (entry-point + persistence + card button) — deferred until the export-only editor is proven; this cycle only spikes feasibility
- Polygon / freehand-vector zones — FP2 zones are grid bitmasks; rectangle/paint on the grid is sufficient
- Multi-room storage / server backend (the SHS tool's Express+JSON layer) — the card is self-contained in HA
- Automatic furniture→interference inference — the existing "Auto Interference Detection" button covers this
- Firmware changes for the export-only flow — export/import work purely client-side over existing actions

## Context

- **Origin issue:** github.com/tigercraft4/esphome_fp2 issue #1 — "GUI zone editor (draw zones on a grid, export YAML)".
- **Why not fork the SHS tool** (`notownblues/SHS-Z2M-Presence`): it is tied to Zigbee2MQTT and a *rectangle-in-mm* zone model with a runtime save-to-sensor flow. Our model is a compile-time 14×14 bitmask grid over the ESPHome native API. Only ~30% (the drawing UI) is reusable and the core would need rewriting. Extending the existing `card.js` (~797 lines, already renders grid + decodes targets) is far less work.
- **Coordinate reference** (corner mount `left_corner` = 0x02, per reverse-engineering `PROTOCOL.md` §5.1): raw `X` −400..+400 → `Grid_X = (X+400)/800*14`; raw `Y` 0..800 → `Grid_Y = Y/800*14`. Grid is 14×14 @ 0.5 m/cell = 7 m × 7 m.
- **Target decode** (already in `card.js`): base64 = `count(1)` then per target `id(1), x s16(2), y s16(2), z s16(2), vel s16(2), snr s16(2), class(1), posture(1), active(1)`.
- **Verification surface:** the standalone `fp2-card-test.html` harness is the primary dev/test loop (paint, serialize, export) — no HA or device needed to iterate.
- **Known codebase concerns** (see `.planning/codebase/CONCERNS.md`): grid round-trip has no tests; `parse_ascii_grid` offset mapping (14×14→20×16) is undocumented and fragile — the serializer must match it exactly.

## Constraints

- **Tech stack**: Editor lives in `card.js` (vanilla JS Lovelace card, no build step / framework). Must load in HACS and standalone.
- **Compatibility**: Export must match `parse_ascii_grid` (`components/aqara_fp2/__init__.py`) exactly — exactly 14×14, `X`/`x` active, `.` inactive, spaces ignored.
- **Transport**: Home Assistant native ESPHome API only (existing `hass.callService` / actions). No MQTT.
- **Firmware**: No firmware/C++ changes required for export-only + import. Runtime write is spike-only this cycle.
- **Geometry**: Must honor `mounting_position` and `left_right_reverse` so drawn cells align with the physical room.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Extend `card.js` instead of forking SHS tool | Card already renders grid + decodes targets natively in HA; only ~30% of SHS reusable and needs core rewrite | — Pending |
| Export-only + import in v1; live write deferred to spike | Matches compile-time YAML model; lowest risk path to Core Value | — Pending |
| No MQTT; use HA native API as transport | Card runs inside HA and already uses native actions (`get_map_config`); MQTT adds broker/config/security cost with no gain | — Pending |
| Verify on `fp2-card-test.html` harness | Fast iteration without HA/device; live HA confirmation at the end | — Pending |
| Include Import (`get_map_config`) in v1 | Round-trip editing of the current device layout is high value and the action already exists | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-07-12 after initialization*
