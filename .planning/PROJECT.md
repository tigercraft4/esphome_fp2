# Aqara FP2 ESPHome — GUI Zone Editor

## What This Is

An ESPHome integration for the Aqara FP2 mmWave presence sensor: reverse-engineered C++ external components that speak the FP2 UART radar protocol, plus a Home Assistant Lovelace card (`card.js`) that renders the 14×14 detection grid and live target tracking, with a **built-in graphical zone editor**: zones and the interference/exit/edge maps can be drawn on a live radar view, configured (type/sensitivity/timeout), exported to a ready-to-paste `zones:` YAML block, and re-imported from the device's current config for further refinement — replacing error-prone hand-editing of 14×14 ASCII grids.

## Core Value

A user can draw a zone by painting cells on the grid and get a valid `zones:` YAML block that flashes without `parse_ascii_grid` errors — no hand-editing of ASCII grids.

**Shipped in v1.0** — confirmed end-to-end by the milestone's integration audit: paint → export produces byte-exact, `parse_ascii_grid`-compatible YAML.

## Current Milestone: v1.1 Runtime Save-to-Sensor

**Goal:** A user can push an edited zone/global-zone straight from the card to the live device and have it survive a reboot without reflashing — on top of a corrected, more complete protocol layer ported from a more mature sibling fork.

**Target features:**
- **Protocol fix:** host-initiated reads (`fp2_read_attr`) use the correct wire opcode (`0x01` request / `0x04` response, fire-and-forget) instead of the wrong one this codebase shipped with — resolves the Phase 6 spike's 5/5 reproducible read timeouts
- Sleep-mode `0x0203` heartbeat keepalive; corrected fall-detection SubID mapping (`0x0121`/`0x0179`/`0x0180`)
- New sensors: per-zone people count (`ZONE_PEOPLE_NUMBER`), `FALL_OVERTIME_REPORT`
- Diagnostics: `debug_mode` verbose logging, optional telnet raw-UART bridge (default off)
- Live write path for editing an existing compiled zone's grid/sensitivity/type (proven mechanics: `enqueue_command_blob2_`, `fp2_write_attr_uint8`, ACK-confirmed) plus the Global Zone
- ESP32-side NVS persistence layer (ESPHome `preferences`) so `check_initialization_()` re-applies the last-saved override on every boot, independent of whatever the radar itself does or doesn't remember
- Card UI "Save to Sensor" action that honestly reflects confirmation level — real read-back-confirmed if the protocol fix restores working reads, "applied, not verified" otherwise
- Explicitly deferred: adding/removing a zone live (needs new zone-ID bookkeeping the codebase doesn't have); radar OTA / firmware-image switching (EXPERIMENTAL even upstream, real bricking risk — see `REQUIREMENTS.md` Out of Scope)

**Key context:** `SAVE_TO_SENSOR_FEASIBILITY.md` (Phase 6 spike, updated 2026-07-14) is the authority for the runtime-write background. Mid-milestone, comparing against `github.com/JameZUK/esphome_fp2_ng` (a more mature sibling fork sharing our common ancestor, `hansihe/esphome_fp2`) found the actual root cause of the read timeouts: our `enqueue_read_` sends wire opcode `0x04` (which the firmware only treats as a *reverse-read response*), when the correct host-initiated read request is opcode `0x01` — their fork does exactly this, fire-and-forget, and its inbound `0x04` handling matches what our dispatcher already does. This is a bug fix, not a hardware limitation as the spike doc first concluded; once fixed, real read-back verification becomes possible, so `RUN-03`'s UI-honesty requirement adapts to whichever confirmation level the protocol actually supports rather than assuming the worst case. The same comparison surfaced several smaller correctness fixes and new sensors, folded into this milestone's `PROTO-*`/`SENSE-*`/`DIAG-*` requirements; their `operating_mode`/radar-OTA subsystem was evaluated and explicitly excluded as too risky (their own repo marks it EXPERIMENTAL/untested).

## Requirements

### Validated

<!-- Shipped and verified in v1.0. -->

- ✓ ESPHome C++ components decode the FP2 UART radar protocol (frame state machine, CRC16) — existing
- ✓ 14×14 grid model with `parse_ascii_grid` validation (ASCII → 40-byte blob) — existing
- ✓ Detection zones with `zone_type` (0x0152), presence + motion binary sensors, motion debounce — existing
- ✓ Lovelace `card.js` renders 14×14 grid, draws zones, decodes live `target_tracking` base64 stream natively in HA (no MQTT) — existing
- ✓ Component exposes current config as JSON via HA action `get_map_config` (`json_get_map_data`) — existing
- ✓ Local diagnostics web server on device — existing
- ✓ Byte-exact `window.FP2Codec` grid↔ASCII/hex serializer, round-trip tested — v1.0 (Phase 1)
- ✓ Edit toggle + independent `editorState`, layer selector, per-layer Clear, live-push freeze seam — v1.0 (Phase 2)
- ✓ Mouse+touch paint/erase (Pointer Events, Bresenham drag interpolation), `window.FP2Geometry` centralized mirror discipline, live target overlay stays aligned while editing — v1.0 (Phase 3)
- ✓ Per-zone `zone_type`/`presence_sensitivity`/`motion_timeout` controls, local-only Add/Remove zone flow, Export YAML (byte-exact, only-set optional keys, injection-safe ids, suspicious-grid confirm gate, 3-tier clipboard fallback) — v1.0 (Phase 4, Core Value)
- ✓ Import current device config (`get_map_config`) merges into editor state without full-replacing; unrecoverable fields (`zone_type`/`motion_timeout`/`global_zone`) surfaced as reset-to-default — v1.0 (Phase 5)
- ✓ README "Zone & Grid Editor" section documents the full workflow — v1.0 (Phase 5)
- ✓ Save-to-Sensor feasibility spike: conditional GO, register sequence + write mechanics documented, reboot-persistence test procedure specified — v1.0 (Phase 6)

### Active

<!-- v1.1 milestone — see REQUIREMENTS.md for full text. -->

- [ ] `PROTO-01..03`: protocol correctness fixes (host-read opcode bug, sleep-mode keepalive, fall-detection SubID mapping) ported from comparison against `JameZUK/esphome_fp2_ng`
- [ ] `SENSE-01..02`: new sensors (`ZONE_PEOPLE_NUMBER`, `FALL_OVERTIME_REPORT`)
- [ ] `DIAG-01..02`: `debug_mode` flag + optional telnet raw-UART diagnostic bridge (default off)
- [ ] `RUN-01..04`: runtime "Save to Sensor" for an *existing compiled* zone/global-zone — live write + ESP32 NVS persistence so the override survives reboot without a reflash, UI honest about confirmation level
- [ ] Capture and commit `images/card_editor_screenshot.png` (README reference exists, image doesn't yet — no headless browser in the dev environment that shipped v1.0)
- [ ] Run the Phase 3 GEOM-03 real-device empirical confirmation (`left_right_reverse`/corner-mount mapping) against a physical FP2

### Out of Scope

- MQTT on the FP2 — card already talks to the device over the native ESPHome API inside HA; a broker adds dependency/config/security surface with no benefit (even for the runtime write path)
- Adding/removing a zone live — requires new live-side zone-ID bookkeeping (`zones_` is populated once at compile time, never mutated at runtime); `RUN-01` is scoped to editing already-compiled zones only
- Protocol-level read-back / persistence verification — confirmed non-functional on real hardware (`SAVE_TO_SENSOR_FEASIBILITY.md` §6); `RUN-01` relies on ACK-of-write + ESP32-side NVS instead of ever reading the radar back
- Polygon / freehand-vector zones — FP2 zones are grid bitmasks; rectangle/paint on the grid is sufficient
- Multi-room storage / server backend (the SHS tool's Express+JSON layer) — the card is self-contained in HA
- Automatic furniture→interference inference — the existing "Auto Interference Detection" button covers this
- Firmware changes for the export-only flow — export/import work purely client-side over existing actions
- Rectangle/flood-fill paint tools, export-to-file download, client-side cache to recover `zone_type`/`motion_timeout` across imports — tracked as v2 accelerator ideas (`ACC-01..03`), not committed to any milestone yet

## Context

- **Origin issue:** github.com/tigercraft4/esphome_fp2 issue #1 — "GUI zone editor (draw zones on a grid, export YAML)".
- **Why not fork the SHS tool** (`notownblues/SHS-Z2M-Presence`): it is tied to Zigbee2MQTT and a *rectangle-in-mm* zone model with a runtime save-to-sensor flow. Our model is a compile-time 14×14 bitmask grid over the ESPHome native API. Only ~30% (the drawing UI) is reusable and the core would need rewriting. Extending the existing `card.js` was far less work — validated: `card.js` grew from ~797 lines to ~2,900 lines entirely in place, no fork, no rewrite.
- **Coordinate reference** (corner mount `left_corner` = 0x02, per reverse-engineering inline comments — no standalone `PROTOCOL.md` file exists in this repo, all protocol knowledge lives as inline C++ comments citing SubID hex values): raw `X` −400..+400 → `Grid_X = (X+400)/800*14`; raw `Y` 0..800 → `Grid_Y = Y/800*14`. Grid is 14×14 @ 0.5 m/cell = 7 m × 7 m.
- **Target decode** (already in `card.js`): base64 = `count(1)` then per target `id(1), x s16(2), y s16(2), z s16(2), vel s16(2), snr s16(2), class(1), posture(1), active(1)`.
- **Verification surface:** `fp2-card-test.html` was the primary dev/test loop for all 6 phases — grew to 5 hand-rolled suites (round-trip, editor, painting&geometry, export&zone-controls, import) with 34+ invariants, zero test framework, zero HA/device dependency to iterate.
- **Live-readback protocol gap (discovered in Phase 4/5):** `get_map_config` only ever returns `{sensitivity, grid, presence_sensor?}` per zone — `zone_type`, `motion_timeout`, and `global_zone` are compile-time-only and never round-trip back to the card. Import must reset these to defaults every session, surfaced to the user via a one-time alert.
- **Geometry double-mirror risk (Phase 3's central concern, confirmed real in code review across Phases 3-5):** every phase that renders both a painted grid and a live overlay must apply `left_right_reverse`'s column mirror through the single centralized `window.FP2Geometry` module — code review caught and fixed 3 separate instances of this exact bug class (hover-preview, zoomed-crop bounds, local-zone selection outline) across the milestone, confirming this was the right risk to prioritize.
- **Save-to-Sensor feasibility (Phase 6, resolved 2026-07-14):** the runtime write primitive (`enqueue_command_blob2_`/`fp2_write_attr_uint8`) is proven live and reliable (ACK every attempt, real device). The reboot-persistence question turned out to be unanswerable at the protocol level — `fp2_read_attr` never gets a response (5/5 timeouts) and `get_map_config` never queries the radar at all. Resolution: `RUN-01` drops protocol-level verification entirely and uses ESP32-side NVS (`preferences`) as the durable store instead, sidestepping the radar's own persistence behavior altogether. See `SAVE_TO_SENSOR_FEASIBILITY.md` §5/§6.
- **Known codebase concerns from v1.0's start** (see `.planning/codebase/CONCERNS.md`): grid round-trip had no tests, `parse_ascii_grid` offset mapping was undocumented and fragile — both resolved by Phase 1's byte-exact `FP2Codec` + round-trip suite.

## Constraints

- **Tech stack**: Editor lives in `card.js` (vanilla JS Lovelace card, no build step / framework). Must load in HACS and standalone.
- **Compatibility**: Export must match `parse_ascii_grid` (`components/aqara_fp2/__init__.py`) exactly — exactly 14×14, `X`/`x` active, `.` inactive, spaces ignored.
- **Transport**: Home Assistant native ESPHome API only (existing `hass.callService` / actions). No MQTT.
- **Firmware**: No firmware/C++ changes required for export-only + import. Runtime write is spike-only this cycle.
- **Geometry**: Must honor `mounting_position` and `left_right_reverse` so drawn cells align with the physical room.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Extend `card.js` instead of forking SHS tool | Card already renders grid + decodes targets natively in HA; only ~30% of SHS reusable and needs core rewrite | ✓ Good — shipped in-place, no fork |
| Export-only + import in v1; live write deferred to spike | Matches compile-time YAML model; lowest risk path to Core Value | ✓ Good — Phase 6's spike confirms a future runtime-write feature is feasible but non-trivial (no atomic commit, needs a live zone registry), validating the deferral |
| No MQTT; use HA native API as transport | Card runs inside HA and already uses native actions (`get_map_config`); MQTT adds broker/config/security cost with no gain | ✓ Good — held through all 6 phases including the Phase 6 spike's transport design |
| Verify on `fp2-card-test.html` harness | Fast iteration without HA/device; live HA confirmation at the end | ✓ Good — every phase's substantive logic was independently re-verified via live Node execution of the actual shipped source against harness fixtures, catching real bugs (e.g. Phase 4/5's zone-key-collision and double-mirror defects) before they reached a device |
| Include Import (`get_map_config`) in v1 | Round-trip editing of the current device layout is high value and the action already exists | ✓ Good — shipped in Phase 5; surfaced a genuine protocol gap (zone_type/motion_timeout/global_zone never echo live) that the editor now handles explicitly |
| Canonical grid + display-time mirror model (`window.FP2Geometry`) | Prevent painted grids and the live target overlay from disagreeing under `left_right_reverse` | ✓ Good — the single centralized mirror module let code review catch 3 real double-mirror defects across Phases 3-5 that would otherwise have shipped |
| Local-only zone additions (`zone:new:*`) not tied to `mapConfig.zones` | The editor needs to let users draw a brand-new zone, but the device only reports zones already in the compiled YAML | ✓ Good — shipped in Phase 4, required careful monotonic-counter bookkeeping (Pitfall 5) to avoid id collisions, later hardened again in Phase 5 after Import could change the device's zone count mid-session |

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
*Last updated: 2026-07-14 — v1.1 Runtime Save-to-Sensor milestone started*
