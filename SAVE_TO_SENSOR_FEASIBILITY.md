# Save-to-Sensor Feasibility Spike

**Status:** Investigation only — no feature shipped by this document.
**Date:** 2026-07-13
**Scope:** SPK-01 — assess whether a runtime "Save to Sensor" write path (pushing
an edited zone/grid config to the FP2 device live, without a reflash) is
feasible, to inform scoping of the deferred `RUN-01` requirement in a future
milestone.
**Recommendation:** **Conditional GO** — gated on a manual reboot-persistence
test (§4). See §5 for the full recommendation and reasoning.

---

## 1. Summary

The mechanism to write a zone's configuration to the FP2 radar at runtime
**already exists and is already proven live** in this codebase, just not
exposed as a dedicated "save this zone" action. `enqueue_command_blob2_()` —
the function that writes `ZONE_MAP` (0x0114), the core per-zone grid
register — is already called on-demand, post-boot, by the existing
`fp2_configure_sleep_mode` Home Assistant action (`configure_sleep_mode()` in
`fp2_component.cpp:306-325`). This is direct, shipped proof that writing this
register outside the boot-time-only path works correctly.

What's genuinely unresolved is **persistence**: does a runtime-written
register survive the radar module's own internal reset? No comment, constant,
or protocol note anywhere in this codebase indicates either way — this is a
hardware-observable fact, not something more code-reading can determine. A
step-by-step test procedure to answer it is given in §4, to be run manually
against a real `fp2-sala` device.

## 2. The Zone Bring-Up Sequence (5 Interdependent Registers)

**Correction to this spike's original framing:** the 5-register zone sequence
does **not** live in `force_detection_config()`. It lives in
`check_initialization_()` (`fp2_component.cpp:448-479`), a method gated by an
`init_done_` flag (`fp2_component.h:438`) that runs **exactly once per boot
cycle**, the first time UART traffic arrives from the radar after a reset
(`fp2_component.cpp:382-388`). `force_detection_config()`
(`fp2_component.cpp:203-226`) is a separate, already-exposed action that
re-sends *global* detection settings and never touches any zone register.

The five registers, defined in `fp2_component.h:148, 183-189`:

| Register | SubID | Shape | Scope |
|----------|-------|-------|-------|
| `ZONE_MAP` | `0x0114` | `[ZoneID(1)] [Grid(40)]`, BLOB2 | Per zone |
| `ZONE_SENSITIVITY` | `0x0151` | `UINT16 (High=ID, Low=Sens)` | Per zone |
| `DETECT_ZONE_TYPE` | `0x0152` | `UINT16 (High=ID, Low=Type)`, optional | Per zone |
| `ZONE_ACTIVATION_LIST` | `0x0202` | 32-byte array, BLOB2 | **Global, not per-zone** |
| `ZONE_CLOSE_AWAY_ENABLE` | `0x0153` | `UINT16 (High=ID, Low=1)` | Per zone |

**Observed order** (from `check_initialization_()`, per zone in `zones_`):
`ZONE_MAP` → `ZONE_SENSITIVITY` → `DETECT_ZONE_TYPE` (if set) → *(after all
zones)* → `ZONE_ACTIVATION_LIST` → *(separate loop)* → `ZONE_CLOSE_AWAY_ENABLE`
for each zone. Whether this order is protocol-mandated or incidental to how
it was originally reverse-engineered is not determinable from this codebase —
treat it as the safe default to replicate; do not reorder without a live test.

**Can a single zone be updated without touching the others?** Yes, for
`ZONE_MAP`/`ZONE_SENSITIVITY`/`DETECT_ZONE_TYPE`/`ZONE_CLOSE_AWAY_ENABLE` —
all four are addressed per-zone by ID. `ZONE_ACTIVATION_LIST` is the
exception: it's a single global 32-byte array (one byte per possible zone ID),
rebuilt by iterating every configured zone. Editing an *existing* zone's
grid/sensitivity/type therefore should not require resending the activation
list; adding or removing a zone (changing which IDs are active) would require
rebuilding and resending the full array, since this protocol has no
incremental "activate zone N" command. This inference is structural, not
tested against real firmware — validate it during the manual test (§4).

## 3. Write Mechanics — `enqueue_command_blob2_()`

Traced end-to-end (`fp2_component.cpp:1361-1380`, `542-651`, `594-633`):

- **Queued, not synchronous.** `enqueue_command_blob2_()` appends to
  `command_queue_` (a `std::deque`) and returns immediately. Sending happens
  in `process_command_queue_()`, called every `loop()` tick.
- **Strict FIFO, one in-flight command, ACK-gated.** No second command sends
  while `waiting_for_ack_attr_id_` is set. Timeout `500ms`, `3` retries; after
  that the command is dropped and logged as `command_ack_failed` in
  `radar_debug`.
- **No atomic commit across a multi-register sequence.** Each command is
  independent. If one drops mid-sequence (e.g. a concurrent
  `fp2_reset_radar` call clears the queue), the zone can end up with a new
  grid but stale sensitivity, or — for a newly-added zone — not yet in the
  activation list at all.
- **Framing is uniform.** `[0x55 Sync][Ver][Seq][OpCode][Len][HeaderChecksum][Payload][CRC16]`
  — identical for every command type; BLOB2 gets a `0x06` DataType byte + 2-byte
  length prefix inside the payload, nothing more.
- **Timing.** Best case (prompt ACKs): well under 100ms for a full 5-register,
  single-zone sequence at 890000 baud. Worst case (every command times out and
  exhausts retries): up to `5 × 3 × 500ms ≈ 7.5s`.

**Would calling this twice in one boot cycle cause a problem?** Existing
precedent says no — `configure_sleep_mode()` already does exactly this today,
live: it calls `enqueue_command_blob2_(ZONE_MAP, empty_zone)` **twice**
(bracketing a `WORK_MODE` change), and is itself invoked on-demand, post-boot,
via the shipped `fp2_configure_sleep_mode` HA action
(`fp2-sala.yaml:47-54`, `example_config.yaml:83-90`). This is the strongest
piece of evidence in this spike: the exact write primitive `RUN-01` would need
is already exercised in production, outside boot-time context, without any
documented issue.

## 4. Reboot-Persistence Test (not executed — manual follow-up)

**No physical FP2 device exists in this development environment.** This
question — does a written register survive the radar module's own reset —
cannot be answered from source code alone; it requires observing actual
hardware behavior. The following procedure is written for whoever has
physical access to a `fp2-sala`-class device to run and record the result.

1. **Baseline.** Call `get_map_config` (via HA Developer Tools → Actions, or
   the card's "Import from Device" button) and record the current `zones`
   array (`sensitivity`, `grid` hex per zone).
2. **Apply a distinguishable test write.** Using the raw diagnostic actions
   already shipped in `example_config.yaml` (`fp2_write_attr_uint16`), write a
   `ZONE_SENSITIVITY` value to an existing zone that differs from its
   compiled default (e.g. if zone 1 compiles as `medium` (`2`), write `high`
   (`3`) via `fp2_write_attr_uint16(attr=0x0151, value=(1<<8)|3)`).
   Sensitivity is the simplest register to test (a single UINT16, not a
   41-byte blob).
3. **Read back before reboot.** Call `get_map_config` again; confirm the
   value reads back as `3`. This proves the write was accepted at the
   protocol level — necessary but not sufficient for persistence.
4. **Power-cycle the FP2 device.** Use a **true power-cycle** (unplug/replug
   the whole unit), not just `fp2_reset_radar`'s GPIO pulse — that path only
   resets the radar module and will be immediately followed by ESPHome's own
   `check_initialization_()` re-pushing the *compiled* config, which would
   mask the result either way.
   - **Important caveat:** `check_initialization_()` runs on **every** boot,
     unconditionally, and re-pushes the compiled-in value regardless of what
     the radar remembers. To get a clean read of radar-side persistence, do
     the read-back (step 5) in the narrow window after the radar resumes
     traffic but you must confirm via `radar_debug` logs whether
     `check_initialization_()` has already re-fired and overwritten your test
     value before you read it, or the test result is inconclusive.
5. **Read back after reboot** via `get_map_config`. If it reads `3` before
   `check_initialization_()` visibly re-runs (confirm via `radar_debug`
   timestamps), that's a positive persistence signal. If it reads `2`
   (the compiled default), either the radar's own storage is volatile, or
   ESPHome's boot-time re-push already happened — check `radar_debug` to
   disambiguate which.
6. **Optional: check the activation list.** If you also test adding/removing
   a zone, read back `ZONE_ACTIVATION_LIST` (`fp2_read_attr(attr=0x0202)`,
   already shipped) to confirm §2's inference that it's a separate,
   wholesale-array register.
7. **Record the result** — update this document's §6 with the outcome before
   scoping `RUN-01`.

## 5. Go/No-Go Recommendation

**CONDITIONAL GO.**

**Reasons to proceed:**
1. The write primitive is already proven live in production
   (`configure_sleep_mode`, shipped as `fp2_configure_sleep_mode`) — not new,
   risky ground.
2. The transport pattern (ESPHome native `api: actions:` → `FP2Component`
   method) is proven by five existing actions, requires no new architecture,
   no MQTT, and no firmware framework changes.
3. Editing an *existing, already-compiled* zone — the most valuable and
   lowest-risk slice of `RUN-01` — doesn't need the riskiest part of the
   sequence (rebuilding the activation list).

**What "conditional" means:**
1. **Persistence is unknown, and — per §6's manual test results — currently
   unanswerable.** Neither `fp2_read_attr` (radar never answers a
   host-initiated read) nor `get_map_config` (never queries the radar; it
   just echoes the compiled YAML) can confirm what value is actually live
   on the device, before or after a reboot. If registers turn out to be
   volatile, `RUN-01` needs an additional shadow-copy/re-apply layer to
   survive radar-only resets — meaningfully more scope than a simple
   write-and-forget action. Resolving this needs either a firmware-side fix
   to the read path or a fallback to indirect behavioral verification.
2. **No atomic commit / no rollback / no read-back** in the existing
   command queue means `RUN-01` must build its own success/failure feedback
   loop back to the card UI relying on ACK-of-write alone (confirmed the
   only reliable signal in §6) — real engineering work with no existing
   precedent to lean on (every existing action is fire-and-forget with no
   response payload, and reads don't work).
3. **Adding/removing a zone live is meaningfully riskier** than editing an
   existing one, since it requires new live-side zone-ID bookkeeping this
   codebase doesn't have today (`zones_` is populated once, at compile time,
   and never mutated at runtime).

**Recommended `RUN-01` scoping:** start with "edit an existing compiled
zone's grid/sensitivity/type live," explicitly deferring "add/remove a zone
live" to a later increment. Require the §4 persistence test to be run and
its result recorded here before committing engineering time to that
milestone's plan.

## 6. Manual Follow-Up: Persistence Test Result

**Status: in progress, real device (`fp2-sala`), 2026-07-14.**

- Added `fp2_read_attr`/`fp2_write_attr_uint8` raw diagnostic actions to
  `fp2-sala.yaml` (not present before), reflashed successfully — confirmed
  live via the HA REST API (`/api/services/esphome`) that
  `fp2_sala_fp2_read_attr`/`fp2_sala_fp2_write_attr_uint8` now exist.
- Attempted `fp2_read_attr(attr=0x0111)` (`PRESENCE_DETECT_SENSITIVITY`,
  global, no zone needed) twice, via direct HA REST API calls
  (`POST /api/services/esphome/fp2_sala_fp2_read_attr`).
- **Result: `command_read_timeout`** both times — device log:
  ```
  [I][aqara_fp2:274]: Queueing raw radar read for presence_detection_sensitivity (0x0111)
  [I][aqara_fp2:133]: command_tx_read presence_detection_sensitivity (0x0111) len=2 data=01 11
  [I][aqara_fp2:133]: command_read_timeout presence_detection_sensitivity (0x0111) len=2 data=01 11
  ```
  The radar never responded to the on-demand READ within the 500ms ACK
  window (confirmed via the `Radar Debug` text_sensor mirroring the same
  log lines). This is a **new finding, not anticipated by the original
  research**: unlike writes (`configure_sleep_mode`'s proven-live `ZONE_MAP`
  writes), an ad-hoc READ of an arbitrary attribute may not be reliably
  answered by the radar outside whatever specific contexts the firmware
  expects reads in (e.g. only right after a corresponding WRITE, or only for
  attributes actively being polled elsewhere).
- **Implication for `RUN-01`:** if reads are unreliable in general (not just
  for this one SubID), the "did my write actually take effect" verification
  loop `RUN-01` needs (Risk 2's success/failure feedback) cannot rely on a
  simple read-back — it may need to lean on ACK-of-the-WRITE alone as the
  only success signal, or investigate why reads time out (wrong attribute,
  wrong read opcode, radar-side gating) before committing to a design.
- **Follow-up round, same day, three more tests:**
  1. `fp2_read_attr(attr=0x0102)` — `RADAR_SW_VERSION`, an attribute the
     radar reports unprompted at boot (i.e. known-good, not some obscure
     SubID). **Result: `command_read_timeout` again.** Rules out "0x0111
     specifically is unsupported" — the timeout is structural, not
     attribute-specific.
  2. `fp2_write_attr_uint8(attr=0x0111, value=3)` (bump
     `PRESENCE_DETECT_SENSITIVITY` from compiled `medium`/2 to `high`/3).
     **Result: `command_ack`, len=0 — succeeded immediately**, consistent
     with `configure_sleep_mode`'s proven-live write path. Writes work;
     reads don't.
  3. `fp2_read_attr(attr=0x0111)` immediately after that write, to test
     whether a read only succeeds in the narrow window right after writing
     the same attribute. **Result: `command_read_timeout` again.** Rules
     that theory out too.
  - **Conclusion: on-demand reads of arbitrary attributes do not work on
    this firmware, full stop** — not a transient issue, not
    attribute-specific, not write-then-read timing. The host-side
    READ/RESPONSE bookkeeping was traced in `fp2_component.cpp` (queues a
    `waiting_for_response_attr_id_`, matches it against incoming
    `OpCode::RESPONSE` frames at the point they arrive) and is implemented
    correctly — the radar itself simply never emits a `RESPONSE` frame for
    a host-initiated read.
  - **Cross-check against the independent reverse-engineering reference**
    ([hansihe/AqaraPresenceSensorFP2ReverseEngineering](https://github.com/hansihe/AqaraPresenceSensorFP2ReverseEngineering),
    `PROTOCOL.md`) complicates this. Its §6 attribute table — derived from
    static disassembly of the firmware's `radar_attribute_table` /
    `subCommandWithOperationValidityTable` — lists **both tested SubIDs as
    `RW`**: `0x0102` (`sw_version`, "R") and `0x0111`
    (`presence_det_sens`, "RW"), where "R" is explicitly defined as
    "Read(4)/Resp(1)" being a permitted operation. If that table is
    accurate, the firmware's own permission check should accept a
    host-initiated `0x04` Read for both attributes — it isn't a
    write-only/read-only-by-design attribute rejecting the request.
    `READ_TIMEOUT_MS` is also 500ms, identical to `ACK_TIMEOUT_MS`, which
    comfortably covers WRITE acks in practice — so the failure isn't an
    under-provisioned timeout window either. This is now an **open,
    unresolved discrepancy** rather than a settled "protocol doesn't
    support host reads" conclusion: either this specific unit's firmware
    revision (`radar_sw_version` reads as `99` in the `sw_version` sensor
    state) behaves differently from the one that reference table was
    reverse-engineered from, there's an undocumented precondition the
    radar requires before it will answer a Read (a specific prior
    sequence, a mode flag, a queue-state requirement), or there's a subtle
    framing mismatch between what this component sends and what the
    firmware's read handler expects that a permission-table read doesn't
    capture. The same reference (§2.2) also documents a **reverse query
    mechanism** — for `device_direction` (`0x0143`) and
    `angle_sensor_data` (`0x0120`), the *radar* queries the *host*
    (`OpCode 0x01`, matching this component's own
    `send_reverse_response_()` at `fp2_component.cpp:669`) — confirming
    that direction of the exchange works as implemented, but that's the
    opposite direction from what `fp2_read_attr` needs. Not a firmware/host
    bug conclusion either way yet — flagged as unresolved, not root-caused.
- **Second, more consequential finding: `get_map_config` cannot be used as
  a read-back oracle at all.** Tracing `FP2Component::get_map_config_json()`
  (`fp2_component.cpp:1494`) shows it deserializes `this->map_config_json_`
  — a string built once from the **compile-time YAML config** — and returns
  it verbatim, with a comment noting runtime data could be layered in "in
  the future" but isn't today. It never issues a live read of the radar.
  This means **the entire §4 test procedure as written is invalid**: step 3
  ("read back before reboot via `get_map_config`, confirm it reads `3`")
  can never succeed, because `get_map_config` will always echo the
  compiled `medium`/2 default regardless of what was actually written live
  to the radar moments earlier.
- **Net effect:** there is currently **no working read-back path** for
  live register state on this firmware — not via ad-hoc `fp2_read_attr`
  (radar doesn't respond), not via `get_map_config` (never asks the radar
  in the first place). The reboot-persistence question from §4 is not just
  unexecuted, it's **currently unanswerable** with existing tooling. The
  only remaining way to observe whether a live write "stuck" — before or
  after a reboot — is indirect behavioral inference (e.g. does the radar's
  actual motion sensitivity visibly change), not a protocol-level check.
- **Next steps (carried to a fresh session):** (1) decide whether to invest
  in figuring out why the radar won't answer host-initiated reads (may
  require re-examining the reverse-read mechanism at
  `send_reverse_response_()` / `fp2_component.cpp:669` — the protocol may
  only support the *radar* initiating reads *of the host*, never the
  reverse); (2) if reads stay unfixable, `RUN-01`'s success/failure
  feedback loop (Risk 2) must rely on ACK-of-write alone, and any
  persistence check must be behavioral, not protocol-level; (3) the live
  test write from this session (`0x0111` → `3`, high sensitivity) was
  reverted back to the compiled `medium`/2 default (`command_ack`
  confirmed) before ending the session, out of caution — not because
  leaving it would have broken anything (a reboot would have overwritten it
  back via `check_initialization_()` regardless, per §2's compiled re-push
  behavior).

## 7. Risks

| # | Risk | Why it happens | Mitigation / what to watch for |
|---|------|-----------------|----------------------------------|
| 1 | Writing while the radar is mid-detection-cycle | No documented "pause detection" / "begin transaction" primitive in the protocol | No static mitigation identified — watch for stale/corrupted zone-presence reports immediately after a live write during the manual test |
| 2 | No atomic commit — partial-write inconsistency if UART drops mid-sequence or a concurrent `fp2_reset_radar` clears the queue | `command_queue_` is a flat FIFO with per-command ACK/retry, no transaction grouping; `perform_reset_()` unconditionally clears the queue | A real implementation should track "sequence in progress" state, block concurrent resets while pending, and verify all ACKs before reporting success to the UI |
| 3 | Live write vs. the next boot-time re-push drifting out of sync | `check_initialization_()` always re-pushes the *compiled* config on every boot, with no concept of "a live override exists" | Treat a runtime save as ephemeral unless the user also re-exports and reflashes the updated YAML — document clearly so a "saved" zone reverting after reboot doesn't surprise users |
| 4 | Adding/removing a zone live is riskier than editing one | Requires rebuilding + resending the 32-byte activation list, which needs a live zone-ID registry that doesn't exist today | Scope `RUN-01` v1 to edit-only; defer live add/remove |

## 8. Illustrative Transport Design Sketch

**Not a working PR — untested, unregistered, omits error handling.** Shown to
give a future `RUN-01` a concrete starting point, following the exact pattern
of `fp2_force_detection_config`/`fp2_configure_sleep_mode`.

### ESPHome YAML action entry

```yaml
# ILLUSTRATIVE — not implemented, not registered.
- action: fp2_save_zone_to_sensor
  variables:
    zone_id: int
    grid_hex: string        # 80 hex chars = 40 bytes, same format get_map_config returns
    sensitivity: int         # 1=low, 2=medium, 3=high
    zone_type: int           # optional; -1 sentinel for "not set"
  then:
    - lambda: |-
        id(fp2).save_zone_to_sensor((uint8_t) zone_id, grid_hex,
                                     (uint8_t) sensitivity, zone_type);
```

### `FP2Component` method sketch

```cpp
// ILLUSTRATIVE — not implemented.
void FP2Component::save_zone_to_sensor(uint8_t zone_id, const std::string &grid_hex,
                                        uint8_t sensitivity, int zone_type) {
  if (grid_hex.size() != 80) {
    ESP_LOGE(TAG, "save_zone_to_sensor: grid_hex must be 80 hex chars, got %u",
             (unsigned) grid_hex.size());
    return;
  }
  std::vector<uint8_t> grid(40);
  for (int i = 0; i < 40; i++) {
    grid[i] = (uint8_t) strtol(grid_hex.substr(i * 2, 2).c_str(), nullptr, 16);
  }

  std::vector<uint8_t> payload;
  payload.push_back(zone_id);
  payload.insert(payload.end(), grid.begin(), grid.end());
  enqueue_command_blob2_(AttrId::ZONE_MAP, payload);

  uint16_t sens_val = (zone_id << 8) | (sensitivity & 0xFF);
  enqueue_command_(OpCode::WRITE, AttrId::ZONE_SENSITIVITY, sens_val);

  if (zone_type >= 0) {
    uint16_t type_val = (zone_id << 8) | ((uint8_t) zone_type & 0xFF);
    enqueue_command_(OpCode::WRITE, AttrId::DETECT_ZONE_TYPE, type_val);
  }

  enqueue_command_(OpCode::WRITE, AttrId::ZONE_CLOSE_AWAY_ENABLE, (uint16_t)((zone_id << 8) | 1));

  // ZONE_ACTIVATION_LIST is intentionally NOT resent here — editing an existing
  // zone shouldn't need it (see §2). A real implementation needs a live-side
  // zone registry to know when a write is an addition (requiring a rebuild)
  // vs. an edit (not requiring one) — zones_ today is compile-time only.
  ESP_LOGW(TAG, "save_zone_to_sensor is illustrative; live zone-addition "
                "bookkeeping is not implemented.");
}
```

### `card.js` call-site sketch

```javascript
// ILLUSTRATIVE — not implemented.
async function saveZoneToSensor(hass, deviceName, zoneId, gridHex, sensitivity, zoneType) {
  const service = `${deviceName}_fp2_save_zone_to_sensor`;
  await hass.callService('esphome', service, {
    zone_id: zoneId,
    grid_hex: gridHex,          // same hex string gridToAscii/asciiToGrid already produce
    sensitivity: sensitivity,
    zone_type: zoneType ?? -1,
  }, undefined, undefined, true);
}
```

This reuses 100% of the existing grid-serialization work from Phases 1-4 on
the card side — no new codec needed.

### Security notes for a real implementation

A real `save_zone_to_sensor()` writes directly to hardware from
runtime-supplied input, unlike the existing compile-time-only path (which
trusts YAML). It must validate `zone_id` is in range (`0-31`, matching the
32-byte activation list), and `sensitivity`/`zone_type` are within their enum
ranges, before building a UART frame — the illustrative sketch above only
checks `grid_hex`'s length. No new authentication boundary is introduced; the
action would ride the same encrypted ESPHome native API as every other
existing `fp2_*` action.

---

*This document is the SPK-01 deliverable for the Aqara FP2 Zone Editor
milestone. See `.planning/phases/06-spike-save-to-sensor-feasibility/` for
the full research trail this report was synthesized from.*
