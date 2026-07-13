/**
 * Aqara FP2 Presence Sensor Card for Home Assistant
 *
 * Configuration options:
 * - entity_prefix: (required) The entity prefix for the FP2 sensor (e.g., "sensor.fp2_living_room")
 * - title: (optional) Card title
 * - display_mode: (optional) "full" or "zoomed" - Default: "full"
 * - show_grid: (optional) Show grid lines - Default: true
 * - show_sensor_position: (optional) Show sensor position marker - Default: true
 * - show_zone_labels: (optional) Show zone labels - Default: true
 * - mounting_position: (optional) Sensor mounting position - Default: from entity or "wall"
 * - report_switch_entity: (optional) Override the live-view switch entity
 * - map_config_service: (optional) Override the ESPHome map-config service name
 * - targets_entity: (optional) Override the target-tracking text sensor entity
 */

// === FP2Codec START (byte-exact port of parse_ascii_grid/grid_to_hex_string) ===
// Source: bit-exact port of components/aqara_fp2/__init__.py parse_ascii_grid (lines 114-176)
// and grid_to_hex_string (line 178). Pure functions only — no DOM/hass dependencies (D-01, GRID-03).
//
// hexToGrid additionally supports the LIVE get_map_config wire format, which is 56 hex
// chars (14 rows) via grid_to_hex_card_format() in fp2_component.cpp, NOT the 80-char
// canonical format below. Because offset_row is always 0, both formats' first 14 rows
// use identical offset_col=2/MSB-first bit math, so one hexToGrid decodes both (D-05).
window.FP2Codec = (function () {
  const ROWS_OUT = 20; // protocol grid rows (only first 14 are ever populated; offset_row=0)
  const COLS_OUT = 16; // protocol grid cols
  const OFFSET_ROW = 0;
  const OFFSET_COL = 2;
  const GRID_SIZE = 14;

  function emptyGrid() {
    return Array.from({ length: GRID_SIZE }, () => Array(GRID_SIZE).fill(0));
  }

  function asciiToGrid(ascii) {
    const lines = (ascii || "")
      .trim()
      .split("\n")
      .map((l) => l.replace(/ /g, ""))
      .filter((l) => l.length > 0);
    if (lines.length !== GRID_SIZE) {
      console.warn(`[FP2 Card] asciiToGrid: expected ${GRID_SIZE} rows, got ${lines.length}, using empty grid`);
      return emptyGrid();
    }
    const grid = [];
    for (let r = 0; r < GRID_SIZE; r++) {
      if (lines[r].length !== GRID_SIZE) {
        console.warn(`[FP2 Card] asciiToGrid: row ${r + 1} must have ${GRID_SIZE} chars, got ${lines[r].length}, using empty grid`);
        return emptyGrid();
      }
      grid.push(Array.from(lines[r]).map((ch) => (ch === "x" || ch === "X" ? 1 : 0)));
    }
    return grid;
  }

  function gridToAscii(grid) {
    return grid.map((row) => row.map((v) => (v ? "X" : ".")).join("")).join("\n");
  }

  function gridToHex(grid) {
    // grid: 14x14 array of 0/1 -> full 40-byte / 80-hex-char protocol blob.
    const bytes = new Uint8Array(ROWS_OUT * 2);
    for (let r = 0; r < GRID_SIZE; r++) {
      const outR = r + OFFSET_ROW;
      let rowVal = 0;
      for (let c = 0; c < GRID_SIZE; c++) {
        if (grid[r][c]) {
          const outC = c + OFFSET_COL;
          rowVal |= 1 << (15 - outC); // MSB-first: col 0 -> bit 15
        }
      }
      bytes[outR * 2] = (rowVal >> 8) & 0xff; // high byte (Big-Endian)
      bytes[outR * 2 + 1] = rowVal & 0xff; // low byte
    }
    return Array.from(bytes)
      .map((b) => b.toString(16).padStart(2, "0"))
      .join("");
  }

  function hexToGrid(hex) {
    // Accepts either the 80-char (20-row) canonical protocol blob or the live
    // get_map_config action's 56-char (14-row) response. Defensive-parse-never-throw
    // (matches the previous parseGrid idiom): malformed input warns and returns a
    // 14x14 zero grid, it never throws.
    // A legitimately-absent optional grid (firmware omits interference/exit/edge
    // grids when unconfigured) is the normal empty-grid case — return silently
    // to avoid a console.warn on every render cycle (WR-03). Reserve warnings
    // for genuinely present-but-malformed non-empty input.
    if (hex == null || hex === "") {
      return emptyGrid();
    }
    if (typeof hex !== "string" || hex.length % 4 !== 0) {
      console.warn(`[FP2 Card] hexToGrid: invalid/malformed hex (length ${typeof hex === "string" ? hex.length : typeof hex}), using empty grid`);
      return emptyGrid();
    }
    // Validate the hex alphabet before parsing — parseInt stops at the first
    // non-hex char (e.g. "12zz" -> 0x12, "zzzz" -> NaN) and would silently
    // mis-decode a corrupt-but-correct-length payload (WR-02).
    if (!/^[0-9a-fA-F]*$/.test(hex)) {
      console.warn(`[FP2 Card] hexToGrid: non-hex characters in input, using empty grid`);
      return emptyGrid();
    }
    if (hex.length !== 56 && hex.length !== 80) {
      console.warn(`[FP2 Card] hexToGrid: unexpected hex length ${hex.length} (expected 56 or 80), decoding first ${GRID_SIZE} rows anyway`);
    }
    const availableRows = hex.length / 4;
    const rowsToRead = Math.min(GRID_SIZE, availableRows);
    const grid = emptyGrid();
    for (let r = 0; r < rowsToRead; r++) {
      const rowVal = parseInt(hex.substr(r * 4, 4), 16);
      for (let c = 0; c < GRID_SIZE; c++) {
        const outC = c + OFFSET_COL;
        grid[r][c] = (rowVal >> (15 - outC)) & 1;
      }
    }
    return grid;
  }

  return { asciiToGrid, gridToAscii, gridToHex, hexToGrid };
})();
// === FP2Codec END ===

// === FP2Geometry START (pure mirror/target-transform/cell-walk math for the Phase 3 zone editor) ===
// Standalone pure module (no this/DOM/hass) mirroring the FP2Codec pattern above, so
// fp2-card-test.html can unit-test the mirror (D-05), corner-mount target transform (D-06),
// and Bresenham cell-walk (PAINT-01) math directly, without a live device. This is the
// single highest-risk new geometry in Phase 3 (RESEARCH Pattern 4: "zones and the moving
// dot disagree" bug class) — extracting it as pure functions is what makes the D-07
// self-consistency test (paint math vs. target math) possible.
window.FP2Geometry = (function () {
  const GRID_SIZE = 14;

  // --- Mirror helpers (D-05: leftRightReverse is checked in exactly one place per axis) ---

  function mirrorColumn(col) {
    // Discrete grid column mirror: col -> 13-col. Its own exact inverse.
    return GRID_SIZE - 1 - col;
  }

  function mirrorGridX(gridX) {
    // Continuous 0..14 mirror for the live target overlay (not a discrete column index).
    return GRID_SIZE - gridX;
  }

  function mirrorGrid(grid) {
    // Returns a NEW 14x14 array with each row reversed. MUST NOT mutate the input:
    // the input may alias editorState (D-05), so always slice() before reverse().
    if (!Array.isArray(grid)) {
      console.warn(`[FP2 Card] mirrorGrid: expected an array grid, returning input unchanged`);
      return grid;
    }
    return grid.map((row) => (Array.isArray(row) ? row.slice().reverse() : row));
  }

  function applyGridMirror(grid, leftRightReverse) {
    // The single place leftRightReverse is checked for grid data.
    return leftRightReverse ? mirrorGrid(grid) : grid;
  }

  function applyGridXMirror(gridX, leftRightReverse) {
    // The single place leftRightReverse is checked for the live target overlay.
    return leftRightReverse ? mirrorGridX(gridX) : gridX;
  }

  function invertColumnMirror(displayCol, leftRightReverse) {
    // Exact inverse used by the pointer/click path: the column mirror is its own inverse.
    return leftRightReverse ? mirrorColumn(displayCol) : displayCol;
  }

  // --- Target/cell transforms (D-06/GEOM-03, bounds safety/ASVS V5) ---

  function targetToGridXY(rawX, rawY, mountingPosition) {
    if (!Number.isFinite(rawX) || !Number.isFinite(rawY)) {
      console.warn(`[FP2 Card] targetToGridXY: non-finite raw coordinate, returning grid origin`);
      return { gridX: 0, gridY: 0 };
    }
    if (mountingPosition === "left_upper_corner" || mountingPosition === "right_upper_corner") {
      // Corner mounting modes: 14x14 grid, 7m x 7m area. Raw X in [-400,+400], Y in [0,800].
      // Extracted VERBATIM from drawTargets — preserve the leading negation on rawX
      // (RESEARCH Pattern 5 option a). leftRightReverse is applied separately via
      // applyGridXMirror, never inside this function.
      return { gridX: ((-rawX + 400) / 800.0) * 14.0, gridY: (rawY / 800.0) * 14.0 };
    }
    // Wall mounting mode: coordinate conversion not yet verified (out of scope). Placeholder
    // preserved verbatim from drawTargets.
    return { gridX: rawX * 0.01, gridY: rawY * 0.01 };
  }

  function canvasPosToGridCell(x, y, renderParams) {
    // Extracted verbatim from handleCanvasClick so there is exactly one canvas->grid-cell
    // implementation. Never-throw guard on malformed renderParams/non-finite coordinates;
    // does NOT bounds-clamp the computed index (0..13 guard lives at the paint call site
    // in plan 03-03) so this stays reusable by the live-view logger.
    if (
      !renderParams ||
      !Number.isFinite(renderParams.minX) ||
      !Number.isFinite(renderParams.minY) ||
      !Number.isFinite(renderParams.cellSize) ||
      renderParams.cellSize === 0
    ) {
      console.warn(`[FP2 Card] canvasPosToGridCell: malformed renderParams, returning out-of-range sentinel`);
      return { x: -1, y: -1 };
    }
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      console.warn(`[FP2 Card] canvasPosToGridCell: non-finite canvas position, returning out-of-range sentinel`);
      return { x: -1, y: -1 };
    }
    const { minX, minY, cellSize } = renderParams;
    return {
      x: Math.floor(x / cellSize) + minX,
      y: Math.floor(y / cellSize) + minY,
    };
  }

  function walkCellsBetween(a, b) {
    // Integer Bresenham/DDA walk returning every {x,y} cell from a to b inclusive,
    // 8-connected (no diagonal gaps) — PAINT-01, RESEARCH Pattern 2.
    if (a == null) {
      return [b];
    }
    if (b == null) {
      return [a];
    }
    if (!Number.isFinite(a.x) || !Number.isFinite(a.y) || !Number.isFinite(b.x) || !Number.isFinite(b.y)) {
      console.warn(`[FP2 Card] walkCellsBetween: non-finite coordinate, returning endpoint only`);
      return [b];
    }
    const x0 = Math.round(a.x);
    const y0 = Math.round(a.y);
    const x1 = Math.round(b.x);
    const y1 = Math.round(b.y);
    const cells = [];
    let x = x0;
    let y = y0;
    const dx = Math.abs(x1 - x0);
    const dy = -Math.abs(y1 - y0);
    const sx = x0 < x1 ? 1 : -1;
    const sy = y0 < y1 ? 1 : -1;
    let err = dx + dy;
    // eslint-disable-next-line no-constant-condition
    while (true) {
      cells.push({ x, y });
      if (x === x1 && y === y1) break;
      const e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x += sx;
      }
      if (e2 <= dx) {
        err += dx;
        y += sy;
      }
    }
    return cells;
  }

  return {
    mirrorColumn,
    mirrorGridX,
    mirrorGrid,
    applyGridMirror,
    applyGridXMirror,
    invertColumnMirror,
    targetToGridXY,
    canvasPosToGridCell,
    walkCellsBetween,
  };
})();
// === FP2Geometry END ===

// === Zone Controls & Export helpers START (Phase 4, ZONE-01..03/EXP-01..03) ===
// Top-level pure constant/function, mirroring the FP2Codec/FP2Geometry
// pattern above (no this/DOM/hass dependencies, D-01) so fp2-card-test.html
// can unit-test them directly via window.ZONE_TYPES_JS/window.slugify.

// Ported verbatim from ZONE_TYPES in components/aqara_fp2/__init__.py
// (lines ~101-110). JS needs the enum both directions: dropdown values now,
// and (Plan 04-03) export name lookup via the export YAML builder.
const ZONE_TYPES_JS = {
  none: 0,
  tv: 2,
  green_plant: 10,
  leisure: 11,
  dressing: 13,
  closet: 14,
  desk: 15,
  shower: 23,
  stairs: 36,
};
window.ZONE_TYPES_JS = ZONE_TYPES_JS;

// Reverse lookup (value -> name) for the export YAML builder's zone_type
// name resolution — precomputed once here rather than re-derived per call.
const ZONE_TYPE_NAMES_BY_VALUE = Object.fromEntries(
  Object.entries(ZONE_TYPES_JS).map(([name, value]) => [value, name]),
);

// WR-02 fix: single shared source of truth for the presence-sensitivity
// int->string mapping (matches SENSITIVITY_LEVELS in
// components/aqara_fp2/__init__.py). Previously hand-copied verbatim into
// both getOrSeedZoneMeta() and mergeImportedMapConfig() — hoisted here,
// mirroring the ZONE_TYPES_JS pattern above, so the two call sites can
// never drift out of sync.
const SENSITIVITY_INT_TO_STRING_JS = { 1: "low", 2: "medium", 3: "high" };
window.SENSITIVITY_INT_TO_STRING_JS = SENSITIVITY_INT_TO_STRING_JS;

// Injection-safety chokepoint (Pitfall 6/ASVS V5) every zone export id
// (the zone-id export resolver, Plan 04-03) routes through: a free-form
// user-entered zone label must never reach a YAML `id:` scalar position
// raw, since `id:` is an ESPHome/C++ declare_id — letters/digits/underscore
// only, never starting with a digit. Lowercase, collapse every run of
// non [a-z0-9] into a single "_", strip leading/trailing "_", and prefix a
// leading digit with "z". Never throws on null/undefined (treated as "").
function slugify(label) {
  return (label || "")
    .toString()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .replace(/^(\d)/, "z$1"); // C++/ESPHome ids cannot start with a digit
}
window.slugify = slugify;
// === Zone Controls & Export helpers END ===

class AqaraFP2Card extends HTMLElement {
  constructor() {
    super();
    this.config = {};
    this.displayMode = "full"; // 'full' or 'zoomed'
    this.showGrid = true;
    this.showSensorPosition = true;
    this.showZoneLabels = true;
    this.gridSize = 14;
    // Editor-mode state (Phase 2, EDIT-01/EDIT-02). editorState is a plain
    // object keyed by stable layer strings (interference/exit/edge/zone:<index>)
    // holding a per-row deep copy of the decoded grids while editing=true.
    this.editing = false;
    this.editorState = {};
    this.selectedLayer = null;
    // Per-zone control state (Phase 4, ZONE-01..03). zoneMeta is keyed
    // IDENTICALLY to editorState ("zone:<index>" for device zones,
    // "zone:new:<n>" for locally-added zones — Plan 04-02) and holds the
    // controls editorState (grid-only) does not carry: zoneType,
    // presenceSensitivity, motionTimeoutEnabled/Seconds, label. Seeded
    // lazily per key by getOrSeedZoneMeta() below, never eagerly here —
    // only per-key entries are lazy, this container itself starts empty
    // exactly like editorState.
    this.zoneMeta = {};
    // Global Zone control state (Phase 5, IMP-02, D-03). Unlike zoneMeta
    // (keyed, lazily seeded per zone), there is only ever one global_zone —
    // so this is declared eagerly here, not lazily. presenceSensitivity
    // follows the exact null-sentinel discipline zoneMeta.zoneType already
    // established (Phase 4 Pitfall 3): starts null, stays null until the
    // user actively picks a value, and is checked with !== null (never
    // truthy/falsy) before buildExportYaml() emits the global_zone block —
    // omitting the block entirely is meaningfully different from an
    // explicit "medium", whose value may not match the device's compiled
    // default.
    this.globalZoneMeta = { presenceSensitivity: null };
    // Paint/erase interaction state (Phase 3, PAINT-01/PAINT-02, D-02/D-03).
    // this.paintMode is the mode-toggle's current mode ('paint' or 'erase');
    // this._painting tracks an in-progress pointer drag; this._lastCell is
    // the previous drag sample (for walkCellsBetween interpolation);
    // this._hoverCell is the mouse-only hover-preview cell (UI-SPEC Note 5).
    // WR-03: this._activePointerId records WHICH pointer is currently
    // painting, so a second concurrent pointer (multi-touch) is ignored
    // instead of stomping on _lastCell/_painting mid-drag.
    this.paintMode = "paint";
    this._painting = false;
    this._lastCell = null;
    this._hoverCell = null;
    this._activePointerId = null;
  }

  set hass(hass) {
    this._hass = hass;

    if (!this.content) {
      this.initializeCard();
    }

    this.updateCard();
  }

  setConfig(config) {
    if (!config.entity_prefix) {
      throw new Error("You need to define entity_prefix");
    }
    this.config = config;
    this.displayMode = config.display_mode || "full";
    this.showGrid = config.show_grid !== false;
    this.showSensorPosition = config.show_sensor_position !== false;
    this.showZoneLabels = config.show_zone_labels !== false;
    console.log(`[FP2 Card] Card configured with entity_prefix: ${config.entity_prefix}`);
  }

  getReportTargetsSwitchEntity() {
    if (this.config.report_switch_entity) {
      return this.config.report_switch_entity;
    }

    const deviceName = this.config.entity_prefix.replace(/^[^.]+\./, "");
    return `switch.${deviceName}_report_targets`;
  }

  toggleLiveView() {
    const switchEntity = this.getReportTargetsSwitchEntity();
    const switchState = this._hass.states[switchEntity];

    if (!switchState) {
      console.warn(`[FP2 Card] Switch entity not found: ${switchEntity}`);
      return;
    }

    const service = switchState.state === 'on' ? 'turn_off' : 'turn_on';
    console.log(`[FP2 Card] Toggling live view: ${switchEntity} -> ${service}`);

    this._hass.callService('switch', service, {
      entity_id: switchEntity
    });
  }

  toggleEditMode() {
    if (!this.editing) {
      if (!this.mapConfig) {
        console.warn(`[FP2 Card] Edit blocked: mapConfig not yet loaded`);
        if (this.infoPanel) {
          this.infoPanel.textContent = "Map config not loaded yet — try Edit again in a moment.";
        }
        return;
      }

      // Seed editorState from the currently-decoded grids ONLY if it is
      // still empty (D-02) — this persists in-progress edits across an
      // exit/re-entry within the same card instance (RESEARCH Open Question #1).
      if (!this.editorState || Object.keys(this.editorState).length === 0) {
        const data = this.gatherEntityData();
        // Per-row deep copy (RESEARCH Pitfall 1): grid.map(row => row.slice()),
        // NEVER [...grid] — a shallow copy would alias the inner row arrays
        // with the decoded source grid.
        const editorState = {
          interference: data.interferenceGrid.map((row) => row.slice()),
          exit: data.entryExitGrid.map((row) => row.slice()),
          edge: data.edgeLabelGrid.map((row) => row.slice()),
        };
        (data.zones || []).forEach((zone, index) => {
          editorState[`zone:${index}`] = zone.map.map((row) => row.slice());
        });
        this.editorState = editorState;
        console.log(`[FP2 Card] Editor state seeded from currently-decoded grids`);
      }

      // WR-04 fix: freeze the device zone count once, the first time edit
      // mode is entered for this card instance, instead of letting addZone()
      // re-read a live this.mapConfig.zones.length on every call. mapConfig
      // is fetched once and never re-fetched today, so this is currently
      // equivalent — but addZone()'s own numbering discipline depends on
      // deviceZoneCount staying constant across calls, and reading it live
      // made that an unstated assumption rather than a guarantee.
      if (this._deviceZoneCount == null) {
        this._deviceZoneCount =
          this.mapConfig && Array.isArray(this.mapConfig.zones)
            ? this.mapConfig.zones.length
            : 0;
      }

      this.selectedLayer = this.selectedLayer || "interference";
      // UI-SPEC Note 7: default to Paint mode every time edit mode is
      // (re-)entered, regardless of what mode was active last time.
      this.paintMode = "paint";

      // WR-05: reset pointer/paint state on edit-mode entry. WR-03's
      // `if (this._painting) return;` dedup guard in handlePointerDown only
      // ever clears via handlePointerUp/handlePointerCancel; if that release
      // event is ever lost (setPointerCapture failure + drag leaves canvas
      // bounds, or the OS/browser swallows the terminating pointer event),
      // _painting stays stuck true forever with no recovery. Resetting here
      // makes "exit and re-enter edit mode" a working recovery action
      // instead of requiring a full page/card reload.
      this._painting = false;
      this._lastCell = null;
      this._activePointerId = null;
    }

    this.editing = !this.editing;
    console.log(`[FP2 Card] Edit mode ${this.editing ? "entered" : "exited"}`);
    this.updateEditingAffordance();
    this.updatePaintModeAffordance();
    this.updateCard();
  }

  // Lazily seeds (once per key, pull-based) and returns this.zoneMeta[key]
  // (Phase 4, Pattern 1). Mirrors toggleEditMode()'s seed-once idiom above,
  // but per-key on demand rather than pushed eagerly for every zone up
  // front — callers (renderZoneControlsPanel/the export builder/the grid
  // validator, Plans 04-02/04-03) always go through this
  // single seam so seeding correctness only needs to be proven once here.
  //
  // Live-readback gap (RESEARCH Pattern 1, confirmed by direct source
  // read of FP2Component::json_get_map_data()): a device zone's live
  // get_map_config payload only ever contains {sensitivity, grid,
  // presence_sensor?} — zone_type and motion_timeout are compile-time-only
  // attributes NEVER echoed back live. So for an existing device zone:
  //   - presenceSensitivity CAN and MUST be seeded from the live value —
  //     but from mapConfig.zones[i].sensitivity (a raw int: 1/2/3 per
  //     SENSITIVITY_LEVELS), NEVER from a key literally named
  //     `presence_sensitivity` (that key does not exist live — Pitfall 1).
  //   - zoneType/motionTimeoutEnabled/motionTimeoutSeconds MUST start at
  //     their untouched defaults (null/false/5) every session — there is
  //     no live source for them, full stop (Pitfall 2).
  // zoneType starts null (never 0/""/undefined) so "untouched" and
  // "explicitly none" (ZONE_TYPES_JS.none === 0) stay distinguishable via
  // === null, never a truthy/falsy check (Pitfall 3/D-04).
  getOrSeedZoneMeta(key) {
    if (this.zoneMeta[key]) {
      return this.zoneMeta[key];
    }

    let presenceSensitivity = "medium";
    let label = key;

    if (typeof key === "string" && key.startsWith("zone:") && !key.startsWith("zone:new:")) {
      const index = parseInt(key.slice("zone:".length), 10);
      label = `Zone ${index + 1}`;
      const zoneConf = this.mapConfig && Array.isArray(this.mapConfig.zones)
        ? this.mapConfig.zones[index]
        : undefined;
      if (zoneConf && SENSITIVITY_INT_TO_STRING_JS[zoneConf.sensitivity]) {
        // Read `sensitivity` (live int) — NEVER `presence_sensitivity`
        // (compile-time-only string key, does not exist in this payload).
        presenceSensitivity = SENSITIVITY_INT_TO_STRING_JS[zoneConf.sensitivity];
      } else {
        console.warn(`[FP2 Card] getOrSeedZoneMeta: no usable sensitivity for "${key}", defaulting to medium`);
      }
    }
    // "zone:new:*" keys (Plan 04-02's addZone()) never read mapConfig —
    // always default presenceSensitivity "medium"; addZone() overwrites
    // the label with the real one immediately after seeding.

    this.zoneMeta[key] = {
      zoneType: null,
      presenceSensitivity,
      motionTimeoutEnabled: false,
      motionTimeoutSeconds: 5,
      label,
    };
    return this.zoneMeta[key];
  }

  // ZONE-01/D-01: creates a new local-only zone slot (`zone:new:<n>`) in
  // BOTH editorState (a fresh row-independent 14x14 zero grid, RESEARCH
  // Pitfall 1 idiom) and zoneMeta (defaults), selects it, then ends with the
  // populateLayerSelect()+updateCard() two-call tail clearSelectedLayer()
  // establishes so the <select> and canvas both refresh (Pitfall 4).
  //
  // this._nextLocalZoneNum is lazily initialized to deviceZoneCount + 1 and
  // ONLY ever incremented — it must NEVER be derived from a live count of
  // this.zoneMeta's keys, or an add->remove->add cycle would reuse a number
  // still implied by a surviving zone (Pitfall 5).
  addZone() {
    // WR-04 fix: use the device zone count frozen at edit-mode entry
    // (toggleEditMode()) rather than re-reading a live mapConfig value
    // here — falls back to a live read only if addZone() is somehow
    // invoked before edit mode was ever entered.
    const deviceZoneCount =
      this._deviceZoneCount != null
        ? this._deviceZoneCount
        : this.mapConfig && Array.isArray(this.mapConfig.zones)
          ? this.mapConfig.zones.length
          : 0;
    if (this._nextLocalZoneNum == null) {
      this._nextLocalZoneNum = deviceZoneCount + 1;
    }
    const n = this._nextLocalZoneNum++;
    // CR-01 fix: the key suffix derives PURELY from the monotonic
    // _nextLocalZoneNum counter, never from deviceZoneCount. The old
    // formula (`n - deviceZoneCount - 1`) re-read the live deviceZoneCount
    // on every call, so an Import (mergeImportedMapConfig()) that changes
    // this._deviceZoneCount between two addZone() calls could make this
    // formula recompute a suffix that collides with an existing
    // zone:new:* key from an earlier call, silently clobbering that
    // zone's grid/label. Suffix uniqueness now depends only on
    // _nextLocalZoneNum, which is seeded once and ever-incrementing.
    const key = `zone:new:${n}`;

    this.editorState[key] = Array.from({ length: 14 }, () =>
      Array(14).fill(0),
    );
    this.zoneMeta[key] = {
      zoneType: null,
      presenceSensitivity: "medium",
      motionTimeoutEnabled: false,
      motionTimeoutSeconds: 5,
      label: `Zone ${n}`,
    };
    this.selectedLayer = key;
    console.log(`[FP2 Card] Added local zone: ${key} ("Zone ${n}")`);
    this.populateLayerSelect();
    this.updateCard();
  }

  // D-02: deletes a locally-added zone (`zone:new:*`) from editorState+
  // zoneMeta behind a window.confirm() gate — mirrors clearSelectedLayer()'s
  // confirm-gated mutation shape (guard -> label -> confirm -> mutate ->
  // populateLayerSelect()+updateCard() tail). A device-zone key (`zone:N`
  // without `new:`) warns and is a no-op: device zones can never be removed
  // from the editor.
  removeZone(layerKey) {
    if (typeof layerKey !== "string" || !layerKey.startsWith("zone:new:")) {
      console.warn(
        `[FP2 Card] removeZone blocked: "${layerKey}" is not a locally-added zone — device zones cannot be removed`,
      );
      return;
    }

    const label =
      (this.zoneMeta[layerKey] && this.zoneMeta[layerKey].label) || layerKey;
    if (!window.confirm(`Remove "${label}"? This cannot be undone.`)) {
      return;
    }

    delete this.editorState[layerKey];
    delete this.zoneMeta[layerKey];
    this.selectedLayer = null;
    console.log(`[FP2 Card] Removed local zone: ${layerKey}`);
    this.populateLayerSelect();
    this.updateCard();
  }

  // Injection-safety chokepoint (Pitfall 6/ASVS V5, EXP-01): every exported
  // zone `id:` scalar is derived here, and every path routes through the
  // top-level slugify() so a free-form user-entered label (which may
  // contain `:`/`#`/quotes/newlines) can never reach the YAML `id:`
  // position raw. A locally-added zone (`zone:new:*`) resolves from its
  // zoneMeta label; a device zone (`zone:N`) is purely positional
  // (`zone_{N+1}`, matching the layer selector's own 1-indexed numbering,
  // RESEARCH A2) and never reads a label at all.
  resolveZoneExportId(key) {
    if (typeof key === "string" && key.startsWith("zone:new:")) {
      const meta = this.zoneMeta[key];
      const slug = slugify(meta && meta.label);
      if (slug) {
        return slug;
      }
      // slugify() yielded "" (e.g. an all-punctuation label) — fall back to
      // a stable id derived from the key's own (internally-generated, never
      // user-controlled) suffix rather than leaving an empty id: scalar.
      const suffix = key.slice("zone:new:".length);
      return `zone_${suffix}`;
    }
    if (typeof key === "string" && key.startsWith("zone:")) {
      const index = parseInt(key.slice("zone:".length), 10);
      return `zone_${index + 1}`;
    }
    // Defensive fallback for any unexpected key shape — still routed
    // through slugify so nothing raw ever reaches the id: scalar.
    return slugify(key) || "zone";
  }

  // Pure string-builder (RESEARCH Pattern 4) producing a paste-ready
  // `zones:`/global-grid YAML fragment WITHOUT a top-level `aqara_fp2:`
  // wrapper — children paste directly under the user's existing block
  // (RESEARCH A1). Every grid is serialized via window.FP2Codec.gridToAscii
  // VERBATIM (EXP-01) — no new grid-to-text logic lives here. Only-set
  // optional keys (EXP-02/ZONE-03/D-04/D-05): presence_sensitivity is
  // always emitted, zone_type only when meta.zoneType !== null (never a
  // truthy/falsy check — Pitfall 3), motion_timeout only when
  // motionTimeoutEnabled. Never throws — a malformed grid warns and is
  // skipped (FP2Codec guard idiom).
  buildExportYaml() {
    const pad = (level) => "  ".repeat(level);
    const hasActiveCell = (grid) =>
      Array.isArray(grid) &&
      grid.some((row) => Array.isArray(row) && row.some((cell) => cell));
    const emitGridLines = (grid, keyName, indentLevel) => {
      const out = [`${pad(indentLevel)}${keyName}: |`];
      window.FP2Codec.gridToAscii(grid)
        .split("\n")
        .forEach((row) => out.push(`${pad(indentLevel + 1)}${row}`));
      return out;
    };
    const lines = [];
    // CR-02 fix: resolveZoneExportId() can produce a colliding id (a
    // renamed local zone slugifying to a device zone's positional id, two
    // local zones slugifying identically, or the empty-slug fallback's
    // numbering) — ESPHome requires every declared zone id to be unique or
    // the build fails. Track every id assigned during this export call and
    // append a disambiguating numeric suffix on collision.
    const usedZoneIds = new Set();
    const uniqueZoneId = (candidate) => {
      let id = candidate;
      let n = 2;
      while (usedZoneIds.has(id)) {
        id = `${candidate}_${n++}`;
      }
      if (id !== candidate) {
        console.warn(`[FP2 Card] zone id "${candidate}" collided — renamed to "${id}"`);
      }
      usedZoneIds.add(id);
      return id;
    };
    const GLOBALS = [["interference", "interference_grid"], ["exit", "exit_grid"], ["edge", "edge_grid"]];
    GLOBALS.forEach(([sk, ck]) => {
      const g = this.editorState[sk];
      // WR-03 fix: mirror the zone loop's malformed-grid guard below so the
      // docstring's "a malformed grid warns and is skipped" claim actually
      // holds for the global grids too — should be unreachable given
      // editorState is always built 14x14, but should fail safely/
      // consistently if it ever is not.
      if (!Array.isArray(g)) {
        console.warn(`[FP2 Card] malformed grid: ${sk}`);
        return;
      }
      if (hasActiveCell(g)) lines.push(...emitGridLines(g, ck, 0));
    });
    // Phase 5/D-03/Pitfall 4: only-if-touched global_zone: block, matching
    // CONFIG_SCHEMA's own key order (global_zone precedes zones in
    // components/aqara_fp2/__init__.py). A strict !== null check (never
    // truthy/falsy) so an untouched control emits nothing at all.
    if (this.globalZoneMeta && this.globalZoneMeta.presenceSensitivity !== null) {
      lines.push(`global_zone:`);
      lines.push(`  presence_sensitivity: ${this.globalZoneMeta.presenceSensitivity}`);
    }
    const zoneKeys = Object.keys(this.editorState).filter((k) => k.startsWith("zone:"));
    if (zoneKeys.length > 0) {
      lines.push("zones:");
      zoneKeys.forEach((key) => {
        const grid = this.editorState[key];
        const meta = this.getOrSeedZoneMeta(key);
        lines.push(`  - id: ${uniqueZoneId(this.resolveZoneExportId(key))}`);
        if (Array.isArray(grid)) lines.push(...emitGridLines(grid, "grid", 2));
        else console.warn(`[FP2 Card] malformed grid: ${key}`);
        lines.push(`    presence_sensitivity: ${meta.presenceSensitivity}`);
        const tn = meta.zoneType !== null ? ZONE_TYPE_NAMES_BY_VALUE[meta.zoneType] : null;
        if (tn) lines.push(`    zone_type: ${tn}`);
        if (meta.motionTimeoutEnabled) {
          const seconds = Math.max(1, Math.floor(meta.motionTimeoutSeconds || 5));
          lines.push(`    motion_timeout: ${seconds}s`);
        }
      });
    }

    return lines.join("\n");
  }

  // VAL-01/D-06: returns one human-readable "suspicious" entry per grid
  // (the 3 global maps plus every zone) that is entirely empty (0 active
  // cells) or entirely filled (all 196 cells active) — a normal
  // partially-painted grid produces no entry. A grid that is not a
  // 14x14 array-of-14-arrays is flagged defensively instead of counted
  // (should be unreachable given plans 01/02 always build 14x14 grids).
  // Never throws.
  validateGridsForExport() {
    const suspicious = [];
    const globalLayers = [
      ["interference", "Interference Grid"],
      ["exit", "Exit Grid"],
      ["edge", "Edge Grid"],
    ];
    // IN-01 fix: route through getOrSeedZoneMeta() (like buildExportYaml()
    // does) instead of reading this.zoneMeta[k] directly, so a zone the
    // user never selected in the layer dropdown (never lazily seeded) still
    // shows its friendly "Zone N" label instead of the raw internal key.
    const zoneLayers = Object.keys(this.editorState)
      .filter((k) => k.startsWith("zone:"))
      .map((k) => [k, this.getOrSeedZoneMeta(k).label]);

    [...globalLayers, ...zoneLayers].forEach(([key, label]) => {
      const grid = this.editorState[key];
      const malformed =
        !Array.isArray(grid) ||
        grid.length !== 14 ||
        !grid.every((row) => Array.isArray(row) && row.length === 14);
      if (malformed) {
        suspicious.push(`${label}: malformed grid (not 14x14)`);
        return;
      }
      const active = grid.reduce(
        (sum, row) => sum + row.reduce((s, c) => s + (c ? 1 : 0), 0),
        0,
      );
      if (active === 0) {
        suspicious.push(`${label}: empty (no cells painted)`);
      } else if (active === 196) {
        suspicious.push(`${label}: entirely filled (all 196 cells active)`);
      }
    });

    return suspicious;
  }

  // EXP-03/D-07, Pitfalls 7/8: 3-tier clipboard fallback, never throws
  // (every fallback only warns). Tier 1 is attempted only when secure
  // context + the Clipboard API are both present; tier 2 honors
  // document.execCommand('copy')'s BOOLEAN return; tier 3 leaves the
  // pre-selected textarea visible for a manual copy. The textarea stays
  // populated/visible regardless of which tier "succeeded" — the export
  // click handler is responsible for showing it before this runs.
  async copyToClipboardWithFallback(text, textareaEl) {
    if (
      window.isSecureContext &&
      navigator.clipboard &&
      navigator.clipboard.writeText
    ) {
      try {
        await navigator.clipboard.writeText(text);
        return "clipboard-api";
      } catch (e) {
        console.warn(
          `[FP2 Card] copyToClipboardWithFallback: Clipboard API write failed, falling back`,
          e,
        );
      }
    }

    if (textareaEl && document.execCommand) {
      try {
        textareaEl.focus();
        textareaEl.select();
        if (typeof textareaEl.setSelectionRange === "function") {
          textareaEl.setSelectionRange(0, text.length);
        }
        if (document.execCommand("copy") === true) {
          return "exec-command";
        }
        console.warn(
          `[FP2 Card] copyToClipboardWithFallback: execCommand('copy') returned false, falling back to manual copy`,
        );
      } catch (e) {
        console.warn(
          `[FP2 Card] copyToClipboardWithFallback: execCommand threw, falling back to manual copy`,
          e,
        );
      }
    }

    console.warn(
      `[FP2 Card] copyToClipboardWithFallback: automatic copy unavailable — textarea left visible/pre-selected for manual copy`,
    );
    return "manual-only";
  }

  // Confirm-then-act ordering (Pitfall 9): validateGridsForExport() + a
  // synchronous window.confirm() run and resolve to COMPLETION before
  // buildExportYaml()/the clipboard write ever run — cancelling the
  // confirm is an unconditional early return, so nothing is ever built or
  // copied. Mirrors clearSelectedLayer()'s guard -> confirm -> act shape.
  handleExportClick() {
    const suspicious = this.validateGridsForExport();
    if (suspicious.length > 0) {
      const proceed = window.confirm(
        `The following grids look suspicious:\n\n${suspicious.join("\n")}\n\nExport anyway?`,
      );
      if (!proceed) {
        return;
      }
    }

    const yaml = this.buildExportYaml();
    const textarea = this.querySelector(".export-textarea");
    if (textarea) {
      textarea.value = yaml;
      textarea.style.display = "";
    }
    this.copyToClipboardWithFallback(yaml, textarea);
  }

  // WR-01 fix: returns true on success, false if the catch block fires —
  // lets callers (e.g. handleImportClick()) distinguish a real refresh
  // from a silently-failed one instead of assuming stale this.mapConfig
  // is fresh. Backward compatible: the existing initializeCard() call
  // site (below) already ignores the return value.
  async fetchMapConfig() {
    const deviceName = this.config.entity_prefix.replace(/^[^.]+\./, "");
    const service = this.config.map_config_service || `${deviceName}_get_map_config`;

    try {
      console.log(`[FP2 Card] Fetching map config via service: esphome.${service}`);
      const response = await this._hass.callService('esphome', service, {}, undefined, undefined, true);
      this.mapConfig = response.response;
      console.log(`[FP2 Card] Map config loaded:`, this.mapConfig);
      this.updateCard();
      return true;
    } catch (e) {
      console.error(`[FP2 Card] Failed to fetch map config:`, e);
      return false;
    }
  }

  // Phase 5 Import (D-02): overwrites the 3 global grids + every existing
  // device zone's grid/sensitivity from a fresh this.mapConfig, resets
  // never-recoverable zoneType/motionTimeout*/globalZoneMeta EVERY import
  // (Pitfall 2), deletes orphaned zone:N keys on a zone-count decrease from
  // BOTH editorState and zoneMeta (Pitfall 1), and preserves zone:new:* by
  // construction. Reuses gatherEntityData() (same hexToGrid decode path
  // toggleEditMode() uses) — never a second decoder. Never throws — a bad
  // zone grid warns and is skipped. Returns the fresh device zone count.
  mergeImportedMapConfig() {
    if (!this.mapConfig) {
      console.warn(`[FP2 Card] mergeImportedMapConfig: no mapConfig to merge from`);
      return 0;
    }

    const data = this.gatherEntityData();

    // Per-row deep copy — never alias gatherEntityData()'s row arrays.
    this.editorState.interference = data.interferenceGrid.map((row) => row.slice());
    this.editorState.exit = data.entryExitGrid.map((row) => row.slice());
    this.editorState.edge = data.edgeLabelGrid.map((row) => row.slice());

    const deviceZones = Array.isArray(this.mapConfig.zones) ? this.mapConfig.zones : [];

    deviceZones.forEach((zoneConf, index) => {
      const key = `zone:${index}`;

      if (data.zones[index] && Array.isArray(data.zones[index].map)) {
        this.editorState[key] = data.zones[index].map.map((row) => row.slice());
      } else {
        console.warn(`[FP2 Card] mergeImportedMapConfig: missing/malformed grid for "${key}", skipping grid overwrite`);
      }

      // zoneType/motionTimeout* have no live source (Pitfall 2) — reset
      // EVERY import, even if set immediately before. Sensitivity IS
      // recoverable: read the raw `sensitivity` int (getOrSeedZoneMeta()'s
      // table above), never the compile-time-only string key.
      this.zoneMeta[key] = {
        zoneType: null,
        presenceSensitivity: SENSITIVITY_INT_TO_STRING_JS[zoneConf.sensitivity] || "medium",
        motionTimeoutEnabled: false,
        motionTimeoutSeconds: 5,
        label: (this.zoneMeta[key] && this.zoneMeta[key].label) || `Zone ${index + 1}`,
      };
    });

    // Deletion reconciliation (Pitfall 1): a zone-count DECREASE deletes
    // the orphaned zone:N (non-new) key from BOTH objects, or
    // buildExportYaml() would silently re-export a zone the device no
    // longer reports.
    Object.keys(this.editorState)
      .filter((k) => k.startsWith("zone:") && !k.startsWith("zone:new:"))
      .forEach((k) => {
        const index = parseInt(k.slice("zone:".length), 10);
        // IN-01 fix: an unreachable-today but malformed key (e.g. "zone:"
        // or "zone:abc") would parseInt to NaN, and `NaN >= n` is always
        // false, silently surviving deletion instead of being flagged —
        // guard explicitly for consistency with this file's otherwise
        // defensive style (hexToGrid, validateGridsForExport).
        if (!Number.isFinite(index)) {
          console.warn(`[FP2 Card] mergeImportedMapConfig: malformed zone key "${k}", deleting`);
          delete this.editorState[k];
          delete this.zoneMeta[k];
          return;
        }
        if (index >= deviceZones.length) {
          delete this.editorState[k];
          delete this.zoneMeta[k];
        }
      });

    // Never recoverable from the device — reset every import (D-02/D-03).
    this.globalZoneMeta = { presenceSensitivity: null };
    this._deviceZoneCount = deviceZones.length;

    console.log(`[FP2 Card] mergeImportedMapConfig: merged ${deviceZones.length} device zone(s)`);

    this.populateLayerSelect();
    this.updateCard();

    return deviceZones.length;
  }

  // Phase 5 Import (D-01/D-04): confirm -> await fetchMapConfig() ->
  // mergeImportedMapConfig() -> window.alert(), strictly in that order. A
  // cancelled confirm() is an unconditional early return BEFORE any side
  // effect — mirrors clearSelectedLayer()/handleExportClick()'s
  // guard-then-confirm-then-act shape, extended to the async case.
  // window.alert() (not the info panel) is used because updateInfoPanel()
  // rebuilds innerHTML on every updateCard()/hass tick and would wipe a
  // panel message before the user reads it.
  async handleImportClick() {
    if (
      !window.confirm(
        "Import will overwrite the global grids and every existing device zone's grid/sensitivity with the device's current configuration. zone_type, motion_timeout, and Global Zone sensitivity will be reset (the device can't report these). Locally-added zones are kept. Continue?",
      )
    ) {
      return;
    }

    // WR-01 fix: a failed re-fetch leaves this.mapConfig stale (whatever
    // it was before the click), so merging/showing success on failure
    // would silently "re-import" stale data with a confident success
    // alert. Skip the merge entirely and show a distinct failure alert.
    const fetchOk = await this.fetchMapConfig();
    if (!fetchOk) {
      window.alert(
        "Import failed — could not fetch the device's current configuration. Try again.",
      );
      return;
    }
    const importedCount = this.mergeImportedMapConfig();
    window.alert(
      `Imported ${importedCount} zone(s) from the device. Note: zone_type, motion_timeout, and Global Zone sensitivity can't be read from the device and were reset — re-set them if needed before exporting.`,
    );
  }

  initializeCard() {
    this.innerHTML = `
      <ha-card>
        <div class="card-header">
          <div class="name">${this.config.title || "Aqara FP2 Presence Sensor"}</div>
          <span class="editing-badge">EDITING</span>
          <div class="controls">
            <button class="live-view-toggle" title="Toggle Live View (Target Reporting)">
              <ha-icon icon="mdi:eye"></ha-icon>
            </button>
            <button class="edit-toggle" title="Enter Edit Mode">
              <ha-icon icon="mdi:pencil"></ha-icon> Edit
            </button>
          </div>
        </div>
        <div class="card-content">
          <canvas id="fp2-canvas"></canvas>
          <div class="editor-controls">
            <span class="editor-controls-label">Layer:</span>
            <select class="layer-select"></select>
            <button class="paint-mode-toggle mode-paint" title="Painting — click/drag to fill cells. Hold Shift or right-click to erase.">
              <ha-icon icon="mdi:brush"></ha-icon> Paint
            </button>
            <button class="clear-layer-btn" title="Clear the currently selected layer">Clear Layer</button>
            <button class="add-zone-btn" title="Add a new local zone">+ Add Zone</button>
            <button class="export-yaml-btn" title="Build a zones:/global-grid YAML block from the current edits and copy it">Export YAML</button>
            <button class="import-config-btn" title="Re-fetch the device's current config and merge it into the editor (overwrites global grids + existing zone grids/sensitivity; keeps locally-added zones)">Import from Device</button>
            <label class="global-zone-field">
              Global Zone
              <select class="global-zone-sensitivity-select">
                <option value="">-- not set --</option>
                <option value="low">Low</option>
                <option value="medium">Medium</option>
                <option value="high">High</option>
              </select>
            </label>
          </div>
          <div class="zone-controls">
            <span class="zone-controls-label">Zone:</span>
            <input type="text" class="zone-label-input" placeholder="Zone label" />
            <label class="zone-controls-field">
              Type
              <select class="zone-type-select">
                <option value="">-- not set --</option>
                <option value="0">None</option>
                <option value="2">TV</option>
                <option value="10">Green Plant</option>
                <option value="11">Leisure</option>
                <option value="13">Dressing</option>
                <option value="14">Closet</option>
                <option value="15">Desk</option>
                <option value="23">Shower</option>
                <option value="36">Stairs</option>
              </select>
            </label>
            <label class="zone-controls-field">
              Sensitivity
              <select class="zone-sensitivity-select">
                <option value="low">Low</option>
                <option value="medium">Medium</option>
                <option value="high">High</option>
              </select>
            </label>
            <label class="zone-controls-field zone-timeout-field">
              <input type="checkbox" class="zone-timeout-checkbox" />
              Motion timeout
            </label>
            <input type="number" class="zone-timeout-number" min="1" step="1" />
            <span class="zone-timeout-suffix">seconds</span>
            <button class="zone-remove-btn" title="Remove this local zone">Remove Zone</button>
          </div>
          <div class="export-panel">
            <span class="export-panel-caption">Paste below your existing aqara_fp2: configuration</span>
            <textarea class="export-textarea" readonly rows="10" style="display: none;"></textarea>
          </div>
          <div class="info-panel"></div>
        </div>
      </ha-card>
      <style>
        ha-card {
          padding: 16px;
        }
        .card-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 16px;
        }
        .card-header .name {
          font-size: 24px;
          font-weight: 500;
        }
        .card-header .controls {
          display: flex;
          gap: 8px;
        }
        .card-header button {
          background: none;
          border: 1px solid var(--divider-color);
          border-radius: 4px;
          padding: 8px;
          cursor: pointer;
          color: var(--primary-text-color);
        }
        .card-header button:hover {
          background: var(--secondary-background-color);
        }
        .card-header button.active {
          background: var(--primary-color);
          color: var(--text-primary-color);
        }
        .edit-toggle {
          display: flex;
          align-items: center;
          gap: 4px;
        }
        .editing-badge {
          display: none;
          background: var(--primary-color);
          color: var(--text-primary-color);
          font-size: 12px;
          font-weight: 500;
          padding: 2px 8px;
          border-radius: 999px;
          text-transform: uppercase;
          letter-spacing: 0.5px;
        }
        .card-content {
          display: flex;
          flex-direction: column;
          gap: 16px;
        }
        #fp2-canvas {
          width: 100%;
          height: auto;
          max-width: 100%;
          display: block;
          border: 1px solid var(--divider-color);
          border-radius: 4px;
          background: var(--card-background-color);
          box-sizing: border-box;
        }
        #fp2-canvas.editing-active {
          border-color: var(--primary-color);
          border-width: 2px;
          cursor: crosshair;
          touch-action: none;
        }
        .editor-controls {
          display: none;
          align-items: center;
          gap: 8px;
          font-size: 14px;
        }
        .editor-controls .layer-select {
          background: none;
          border: 1px solid var(--divider-color);
          border-radius: 4px;
          padding: 8px;
          color: var(--primary-text-color);
          font-size: 14px;
        }
        .editor-controls .paint-mode-toggle {
          display: flex;
          align-items: center;
          gap: 4px;
          background: none;
          border: 1px solid var(--divider-color);
          border-radius: 4px;
          padding: 8px;
          cursor: pointer;
          color: var(--primary-text-color);
          font-size: 14px;
        }
        .editor-controls .paint-mode-toggle.mode-paint {
          background: var(--primary-color);
          color: var(--text-primary-color);
          border-color: var(--primary-color);
        }
        .editor-controls .paint-mode-toggle.mode-erase {
          background: var(--error-color, #db4437);
          color: var(--text-primary-color);
          border-color: var(--error-color, #db4437);
        }
        .editor-controls .clear-layer-btn {
          background: none;
          border: 1px solid var(--error-color, #db4437);
          border-radius: 4px;
          padding: 8px;
          cursor: pointer;
          color: var(--error-color, #db4437);
          font-size: 14px;
        }
        .editor-controls .clear-layer-btn:hover {
          background: var(--error-color, #db4437);
          color: var(--text-primary-color);
        }
        .editor-controls .add-zone-btn {
          background: none;
          border: 1px solid var(--primary-color);
          border-radius: 4px;
          padding: 8px;
          cursor: pointer;
          color: var(--primary-color);
          font-size: 14px;
        }
        .editor-controls .add-zone-btn:hover {
          background: var(--primary-color);
          color: var(--text-primary-color);
        }
        .editor-controls .export-yaml-btn {
          background: none;
          border: 1px solid var(--primary-color);
          border-radius: 4px;
          padding: 8px;
          cursor: pointer;
          color: var(--primary-color);
          font-size: 14px;
          font-weight: 500;
        }
        .editor-controls .export-yaml-btn:hover {
          background: var(--primary-color);
          color: var(--text-primary-color);
        }
        .editor-controls .import-config-btn {
          background: none;
          border: 1px solid var(--primary-color);
          border-radius: 4px;
          padding: 8px;
          cursor: pointer;
          color: var(--primary-color);
          font-size: 14px;
          font-weight: 500;
        }
        .editor-controls .import-config-btn:hover {
          background: var(--primary-color);
          color: var(--text-primary-color);
        }
        .editor-controls .global-zone-field {
          display: flex;
          align-items: center;
          gap: 4px;
          color: var(--primary-text-color);
          font-size: 14px;
        }
        .editor-controls .global-zone-sensitivity-select {
          background: none;
          border: 1px solid var(--divider-color);
          border-radius: 4px;
          padding: 8px;
          color: var(--primary-text-color);
          font-size: 14px;
        }
        .zone-controls {
          display: none;
          flex-wrap: wrap;
          align-items: center;
          gap: 8px;
          font-size: 14px;
          padding: 8px;
          border: 1px solid var(--divider-color);
          border-radius: 4px;
          color: var(--primary-text-color);
        }
        .zone-controls .zone-label-input,
        .zone-controls .zone-type-select,
        .zone-controls .zone-sensitivity-select,
        .zone-controls .zone-timeout-number {
          background: none;
          border: 1px solid var(--divider-color);
          border-radius: 4px;
          padding: 6px;
          color: var(--primary-text-color);
          font-size: 14px;
        }
        .zone-controls .zone-controls-field {
          display: flex;
          align-items: center;
          gap: 4px;
        }
        .zone-controls .zone-remove-btn {
          background: none;
          border: 1px solid var(--error-color, #db4437);
          border-radius: 4px;
          padding: 8px;
          cursor: pointer;
          color: var(--error-color, #db4437);
          font-size: 14px;
        }
        .zone-controls .zone-remove-btn:hover {
          background: var(--error-color, #db4437);
          color: var(--text-primary-color);
        }
        .export-panel {
          display: flex;
          flex-direction: column;
          gap: 4px;
        }
        .export-panel-caption {
          font-size: 12px;
          color: var(--secondary-text-color);
        }
        .export-textarea {
          width: 100%;
          box-sizing: border-box;
          font-family: monospace;
          font-size: 12px;
          padding: 8px;
          border: 1px solid var(--divider-color);
          border-radius: 4px;
          background: var(--card-background-color);
          color: var(--primary-text-color);
          resize: vertical;
        }
        .info-panel {
          font-size: 14px;
          color: var(--secondary-text-color);
        }
      </style>
    `;

    this.content = this.querySelector(".card-content");
    this.canvas = this.querySelector("#fp2-canvas");
    this.ctx = this.canvas.getContext("2d");
    this.infoPanel = this.querySelector(".info-panel");

    // Set up event listeners
    this.querySelector(".live-view-toggle").addEventListener("click", () => {
      this.toggleLiveView();
    });

    this.querySelector(".edit-toggle").addEventListener("click", () => {
      this.toggleEditMode();
    });

    // EDIT-03/D-03: layer selector — registered once here (never
    // re-templated) so it survives every updateCard()/hass push.
    this.querySelector(".layer-select").addEventListener("change", (e) => {
      this.selectedLayer = e.target.value;
      console.log(`[FP2 Card] Layer selection changed: ${this.selectedLayer}`);
      // D-03: the single natural swap point for the inline zone-controls
      // panel — show/hide + repopulate it for whatever layer is now selected.
      this.renderZoneControlsPanel();
    });

    // EDIT-04/D-05: Clear Layer button — registered once here.
    this.querySelector(".clear-layer-btn").addEventListener("click", () => {
      this.clearSelectedLayer();
    });

    // D-01: Add Zone button — registered once here.
    this.querySelector(".add-zone-btn").addEventListener("click", () => {
      this.addZone();
    });

    // Export YAML button — registered once here (Pattern 9/D-06).
    this.querySelector(".export-yaml-btn").addEventListener("click", () => {
      this.handleExportClick();
    });

    // Import from Device button — registered once here (Phase 5, D-01).
    this.querySelector(".import-config-btn").addEventListener("click", () => {
      this.handleImportClick();
    });

    // Global Zone control — registered once here (Phase 5, D-03). Static
    // and always present while editing, never re-templated per layer
    // switch, so this is a plain one-time binding like Add Zone/Export
    // above — NOT the .zone-controls delegated-event pattern. Maps the
    // blank placeholder value="" back to the null sentinel (Pitfall 3).
    this.querySelector(".global-zone-sensitivity-select").addEventListener("change", (e) => {
      this.globalZoneMeta.presenceSensitivity = e.target.value === "" ? null : e.target.value;
      console.log(`[FP2 Card] Global Zone presence_sensitivity changed: ${this.globalZoneMeta.presenceSensitivity}`);
    });

    // D-03/Pattern 3: the zone-controls panel's inputs are bound ONCE via
    // event delegation on the static container (never one listener per
    // input) — handleZoneControlChange() dispatches on e.target's class.
    this.querySelector(".zone-controls").addEventListener("change", (e) => {
      this.handleZoneControlChange(e);
    });
    // D-02: Remove Zone is a click inside the same delegated container.
    this.querySelector(".zone-controls").addEventListener("click", (e) => {
      if (e.target.closest(".zone-remove-btn")) {
        this.removeZone(this.selectedLayer);
      }
    });

    // PAINT-02/D-03: paint/erase mode toggle — registered once here, never
    // re-templated; togglePaintMode() flips this.paintMode and updates the
    // button via classList/textContent only.
    this.querySelector(".paint-mode-toggle").addEventListener("click", () => {
      this.togglePaintMode();
    });

    // WR-02: the stale pre-refactor "click" -> handleCanvasClick listener
    // was removed here — the pointer-event handlers below fully supersede
    // it and are mirror-aware, unlike the old inline (non-mirror-aware)
    // click math.
    this.canvas.addEventListener("mousemove", (e) => this.handleCanvasHover(e));

    // PAINT-01/PAINT-02 (D-02/D-03): unified mouse+touch paint/erase via
    // Pointer Events, registered once here alongside (not replacing) the
    // existing click/mousemove listeners above — RESEARCH Pattern 1. These
    // coexist safely because every new handler guards on `this.editing` and
    // only acts while `this._painting` is true.
    this.canvas.addEventListener("pointerdown", (e) => this.handlePointerDown(e));
    this.canvas.addEventListener("pointermove", (e) => this.handlePointerMove(e));
    this.canvas.addEventListener("pointerup", (e) => this.handlePointerUp(e));
    // pointercancel shares the pointerup handler (RESEARCH Pitfall 2 / T-03-D1):
    // both must clear this._painting and release capture the same way.
    this.canvas.addEventListener("pointercancel", (e) => this.handlePointerUp(e));
    // D-03/Pitfall 3: right-click-to-erase must not open the browser's
    // native context menu while editing.
    this.canvas.addEventListener("contextmenu", (e) => {
      if (this.editing) e.preventDefault();
    });

    // UI-SPEC Note 5: clear the hover-preview when the pointer leaves the
    // canvas so no stale preview persists. pointerleave covers pen/touch/
    // mouse uniformly; mouseleave is kept alongside for the existing
    // mousemove-driven hover path (RESEARCH Pattern 1 coexistence).
    this.canvas.addEventListener("pointerleave", () => this.clearHoverPreview());
    this.canvas.addEventListener("mouseleave", () => this.clearHoverPreview());

    // Set up ResizeObserver to handle card resizing
    this.resizeObserver = new ResizeObserver(() => {
      console.log(`[FP2 Card] Container resized, redrawing canvas`);
      this.updateCard();
    });
    this.resizeObserver.observe(this.content);

    // Fetch map configuration from ESPHome service
    this.fetchMapConfig();
  }

  updateCard() {
    if (!this._hass || !this.canvas) {
      console.log(`[FP2 Card] updateCard skipped: hass=${!!this._hass}, canvas=${!!this.canvas}`);
      return;
    }

    console.log(`[FP2 Card] ===== Card update triggered =====`);

    // EDIT-03/D-03: (re)populate the layer selector once mapConfig is known.
    // populateLayerSelect() self-guards against needless rebuilds when the
    // option count already matches (RESEARCH Pitfall 3) — mapConfig is only
    // known after fetchMapConfig()'s async response calls updateCard() again.
    if (this.mapConfig) {
      this.populateLayerSelect();
    }

    const data = this.gatherEntityData();
    // Threaded fresh on every updateCard() so canvasEventToGridCell (D-05)
    // never relies on a stale value across a drag — mirrors the
    // this.renderParams-re-read-fresh discipline (RESEARCH Security Domain).
    this._lastLeftRightReverse = data.leftRightReverse === true;
    if (this.editing) {
      // D-01/EDIT-02 freeze seam: substitute ONLY the four grid-shaped
      // fields with the frozen editorState values; targets/mountingPosition/
      // leftRightReverse stay live so the overlay keeps moving. This lives
      // in updateCard() (not set hass()) so the ResizeObserver redraw path
      // is frozen too (RESEARCH Pitfall 6).
      console.log(`[FP2 Card] Edit mode active: substituting frozen editorState grids into render data`);
      data.interferenceGrid = this.editorState.interference;
      data.entryExitGrid = this.editorState.exit;
      data.edgeLabelGrid = this.editorState.edge;
      // CR-01 fix: locally-added zones (`zone:new:*`) live only in
      // editorState/zoneMeta, never in the live mapConfig-derived
      // data.zones array above — merge them in here so they get painted
      // (drawDetectionZones) and are eligible for the selected-layer
      // outline, exactly like a device zone.
      const localZoneKeys = Object.keys(this.editorState).filter((k) =>
        k.startsWith("zone:new:"),
      );
      data.zones = [
        ...data.zones.map((zone, i) => ({
          ...zone,
          map: this.editorState[`zone:${i}`] || zone.map,
        })),
        ...localZoneKeys.map((key) => ({
          key,
          id: (this.zoneMeta[key] && this.zoneMeta[key].label) || key,
          map: this.editorState[key],
          occupancy: false,
        })),
      ];
      // D-03: keep the inline zone-controls panel in sync with
      // this.selectedLayer on every edit-mode re-render.
      this.renderZoneControlsPanel();
      // Phase 5/D-03: keep the Global Zone <select> in sync too — this
      // covers both a manual reset and plan 05-02's merge (which ends by
      // calling updateCard()).
      this.renderGlobalZoneControl();
    }
    this.updateLiveViewButton();
    this.renderCanvas(data);
    this.updateInfoPanel(data);
  }

  updateLiveViewButton() {
    const switchEntity = this.getReportTargetsSwitchEntity();
    const switchState = this._hass.states[switchEntity];
    const button = this.querySelector(".live-view-toggle");

    if (!button) return;

    if (switchState && switchState.state === 'on') {
      button.classList.add('active');
    } else {
      button.classList.remove('active');
    }
  }

  updateEditingAffordance() {
    const toggle = this.querySelector(".edit-toggle");
    if (toggle) {
      toggle.classList.toggle("active", this.editing);
      toggle.title = this.editing
        ? "Exit Edit Mode and Return to Live View"
        : "Enter Edit Mode";
      toggle.innerHTML = this.editing
        ? `<ha-icon icon="mdi:check"></ha-icon> Done`
        : `<ha-icon icon="mdi:pencil"></ha-icon> Edit`;
    }

    const badge = this.querySelector(".editing-badge");
    if (badge) {
      badge.style.display = this.editing ? "" : "none";
    }

    const editorControls = this.querySelector(".editor-controls");
    if (editorControls) {
      editorControls.style.display = this.editing ? "flex" : "none";
    }

    if (this.canvas) {
      this.canvas.classList.toggle("editing-active", this.editing);
    }
  }

  // PAINT-02/D-03: flips this.paintMode between 'paint' and 'erase' — the
  // button itself is registered once in initializeCard() and only ever
  // updated via classList/textContent (updatePaintModeAffordance()), never
  // re-templated, mirroring the .edit-toggle/toggleEditMode() idiom.
  togglePaintMode() {
    this.paintMode = this.paintMode === "erase" ? "paint" : "erase";
    console.log(`[FP2 Card] Paint mode toggled: ${this.paintMode}`);
    this.updatePaintModeAffordance();
  }

  updatePaintModeAffordance() {
    const toggle = this.querySelector(".paint-mode-toggle");
    if (!toggle) return;

    const isErase = this.paintMode === "erase";
    toggle.classList.toggle("mode-paint", !isErase);
    toggle.classList.toggle("mode-erase", isErase);
    toggle.title = isErase
      ? "Erasing — click/drag to clear cells. Click to switch back to Paint."
      : "Painting — click/drag to fill cells. Hold Shift or right-click to erase.";
    toggle.innerHTML = isErase
      ? `<ha-icon icon="mdi:eraser"></ha-icon> Erase`
      : `<ha-icon icon="mdi:brush"></ha-icon> Paint`;
  }

  // Phase 5/D-03: keeps the standalone Global Zone <select> in sync with
  // this.globalZoneMeta. Mirrors renderZoneControlsPanel()'s
  // query-node/set-.value-only render shape — this NEVER re-templates with
  // innerHTML, it only sets .value on the bound-once node from
  // initializeCard(). null -> "" (the blank placeholder), matching the
  // change listener's inverse "" -> null mapping (Pitfall 3).
  renderGlobalZoneControl() {
    const select = this.querySelector(".global-zone-sensitivity-select");
    if (!select || !this.globalZoneMeta) return;
    select.value = this.globalZoneMeta.presenceSensitivity === null ? "" : this.globalZoneMeta.presenceSensitivity;
  }

  // D-03: shows/hides + repopulates the static .zone-controls panel for
  // whatever this.selectedLayer currently is. Mirrors
  // updateEditingAffordance()'s query-node/set-.value-.style-only render
  // shape — this NEVER re-templates with innerHTML, it only sets
  // .value/.checked/.disabled/.style on the bound-once nodes from
  // initializeCard() (Pattern 3).
  renderZoneControlsPanel() {
    const panel = this.querySelector(".zone-controls");
    if (!panel) return;

    const key = this.selectedLayer;
    if (typeof key !== "string" || !key.startsWith("zone:")) {
      panel.style.display = "none";
      return;
    }
    panel.style.display = "flex";

    const meta = this.getOrSeedZoneMeta(key);

    const labelInput = panel.querySelector(".zone-label-input");
    if (labelInput) {
      labelInput.value = meta.label;
      // WR-01/D-01 fix: only locally-added zones have an editable label
      // concept in this design — a device zone's identity comes from the
      // device, not a locally-editable label, and populateLayerSelect()'s
      // device-zone branch never reads zoneMeta[key].label anyway (it
      // always derives from presence_sensor/positional index), so leaving
      // this editable for device zones silently accepted edits with zero
      // visible effect on the dropdown.
      labelInput.disabled = !key.startsWith("zone:new:");
    }

    const typeSelect = panel.querySelector(".zone-type-select");
    // === null (never truthy/falsy) so the blank placeholder value=""
    // (untouched) stays distinct from None's value="0" (explicitly set,
    // Pitfall 3/D-04).
    if (typeSelect) {
      typeSelect.value = meta.zoneType === null ? "" : String(meta.zoneType);
    }

    const sensitivitySelect = panel.querySelector(".zone-sensitivity-select");
    if (sensitivitySelect) sensitivitySelect.value = meta.presenceSensitivity;

    const timeoutCheckbox = panel.querySelector(".zone-timeout-checkbox");
    if (timeoutCheckbox) timeoutCheckbox.checked = meta.motionTimeoutEnabled;

    const timeoutNumber = panel.querySelector(".zone-timeout-number");
    if (timeoutNumber) {
      timeoutNumber.disabled = !meta.motionTimeoutEnabled;
      timeoutNumber.value = meta.motionTimeoutSeconds;
    }

    const removeBtn = panel.querySelector(".zone-remove-btn");
    if (removeBtn) {
      // D-02: Remove is only ever offered for locally-added zones — device
      // zones (zone:N) can never be deleted from the editor.
      removeBtn.style.display = key.startsWith("zone:new:") ? "" : "none";
    }
  }

  // D-04/D-05: delegated change handler for the .zone-controls panel
  // (bound once in initializeCard(), Pattern 3). Writes edits back into
  // zoneMeta[this.selectedLayer] via getOrSeedZoneMeta() so edits and
  // export (Plan 04-03) always read the same source of truth.
  handleZoneControlChange(e) {
    const key = this.selectedLayer;
    if (typeof key !== "string" || !key.startsWith("zone:")) return;

    const meta = this.getOrSeedZoneMeta(key);
    const target = e.target;

    try {
      if (target.classList.contains("zone-type-select")) {
        // Blank placeholder ("") means "untouched" -> null; every other
        // value is the numeric ZONE_TYPES_JS enum, including None ("0").
        meta.zoneType = target.value === "" ? null : parseInt(target.value, 10);
      } else if (target.classList.contains("zone-sensitivity-select")) {
        meta.presenceSensitivity = target.value;
      } else if (target.classList.contains("zone-timeout-checkbox")) {
        meta.motionTimeoutEnabled = target.checked;
        // Toggle the seconds input's disabled state to match.
        this.renderZoneControlsPanel();
      } else if (target.classList.contains("zone-timeout-number")) {
        // WR-02 fix: `parseInt("0", 10) || 5` treats a valid "0" the same
        // as NaN (both falsy), silently discarding the user's explicit "0"
        // for an unrelated default of 5 instead of clamping to the field's
        // own declared min="1". Distinguish "not a number" (empty/NaN,
        // falls back to 5) from "a valid but out-of-range number" (clamped
        // to the field's floor of 1).
        const parsed = parseInt(target.value, 10);
        meta.motionTimeoutSeconds = Number.isFinite(parsed)
          ? Math.max(1, parsed)
          : 5;
      } else if (target.classList.contains("zone-label-input")) {
        meta.label = target.value;
        // The label is echoed into the layer <select> option text.
        this.populateLayerSelect();
      }
    } catch (err) {
      console.error(`[FP2 Card] handleZoneControlChange failed:`, err);
    }
  }

  // EDIT-03/D-03: (re)builds the .layer-select <option>s from this.mapConfig
  // — 3 fixed layers (interference/exit/edge) plus one option per configured
  // zone, enumerated dynamically (never hard-coded). Real devices never emit
  // presence_sensor (RESEARCH Pitfall 4), so the "Zone N" (1-indexed) label
  // fallback is the PRIMARY production path, not a rare edge case. The zone's
  // array INDEX is always the stable value/key (`zone:<index>`); a
  // presence_sensor name, when present, is used only for the display label.
  populateLayerSelect() {
    const select = this.querySelector(".layer-select");
    if (!select || !this.mapConfig) {
      return;
    }

    const zones = Array.isArray(this.mapConfig.zones)
      ? this.mapConfig.zones
      : [];
    const fixedLayers = [
      ["interference", "Interference Grid"],
      ["exit", "Exit Grid"],
      ["edge", "Edge Grid"],
    ];
    const zoneLayers = zones.map((zoneConfig, index) => [
      `zone:${index}`,
      zoneConfig.presence_sensor || `Zone ${index + 1}`,
    ]);
    // Plan 04-02/D-01: local-only zones (zone:new:*) are listed after every
    // device zone. Labels come from zoneMeta[key].label (user-editable free
    // text) — kept generic over `desired` below so the existing diff guard/
    // build loop/previousValue reconcile cover Add/Remove for free (Pitfall 4).
    const localZoneLayers = Object.keys(this.zoneMeta || {})
      .filter((k) => k.startsWith("zone:new:"))
      .map((k) => [k, this.zoneMeta[k].label || k]);
    const desired = [...fixedLayers, ...zoneLayers, ...localZoneLayers];

    // Guard against needless rebuilds on every hass push (RESEARCH Pitfall 3).
    // Compare both option VALUES and LABELS (not just count) so a same-count
    // zone change (rename/replace) still triggers a rebuild instead of leaving
    // stale labels on screen.
    const unchanged =
      select.options.length === desired.length &&
      desired.every(
        ([value, label], i) =>
          select.options[i].value === value &&
          select.options[i].textContent === label,
      );
    if (unchanged) {
      return;
    }

    const previousValue = this.selectedLayer;

    // Clear existing children without innerHTML (Security Domain V5 —
    // avoid any HTML-injection-shaped code path, defense-in-depth).
    while (select.firstChild) {
      select.removeChild(select.firstChild);
    }

    desired.forEach(([value, label]) => {
      // document.createElement + .textContent (NOT innerHTML string concat)
      // so a config-sourced name containing "<"/"&" cannot inject markup.
      const option = document.createElement("option");
      option.value = value;
      option.textContent = label;
      select.appendChild(option);
    });

    // Preserve the current selection across a rebuild if it still matches
    // an option; otherwise the <select> falls back to its default (first
    // option). Keep this.selectedLayer in lock-step with whatever the DOM
    // actually shows — programmatically setting select.value does NOT fire the
    // change listener, so without this reconcile clearSelectedLayer() could
    // confirm one layer's label but wipe a different (stale) layer (WR-01).
    if (
      previousValue &&
      Array.from(select.options).some((o) => o.value === previousValue)
    ) {
      select.value = previousValue;
      this.selectedLayer = previousValue;
    } else {
      this.selectedLayer = select.value || null;
    }

    console.log(
      `[FP2 Card] Layer select populated: ${select.options.length} options (3 fixed + ${zones.length} device zone(s) + ${localZoneLayers.length} local zone(s))`,
    );
  }

  // EDIT-04/D-05: empties ONLY editorState[selectedLayer], behind a
  // window.confirm() gate. Other layers are left untouched. Cancel is a
  // no-op. window.confirm() is intentionally synchronous/blocking — this is
  // the locked D-05 approach, not a bug to "fix" into an async modal
  // (RESEARCH Pitfall 7).
  clearSelectedLayer() {
    if (!this.selectedLayer) {
      console.warn(`[FP2 Card] Clear blocked: no layer selected`);
      return;
    }

    const select = this.querySelector(".layer-select");
    const label =
      select && select.selectedIndex >= 0 && select.options[select.selectedIndex]
        ? select.options[select.selectedIndex].textContent
        : this.selectedLayer;

    if (!window.confirm(`Clear "${label}"? This cannot be undone.`)) {
      return;
    }

    // Array.from(...) builds 14 independent row arrays — never share a
    // single row reference across the grid (RESEARCH Pitfall 1).
    this.editorState[this.selectedLayer] = Array.from({ length: 14 }, () =>
      Array(14).fill(0),
    );
    console.log(`[FP2 Card] Cleared layer: ${this.selectedLayer}`);
    this.updateCard();
  }

  gatherEntityData() {
    const prefix = this.config.entity_prefix;
    const hass = this._hass;

    console.log(`[FP2 Card] Starting entity data gathering with prefix: ${prefix}`);

    // Helper to safely get entity state
    const getEntityState = (entityId) => {
      const state = hass.states[entityId];
      if (state) {
        console.log(`[FP2 Card] ✓ Entity found: ${entityId} = "${state.state}"`);
        return state.state;
      } else {
        console.warn(`[FP2 Card] ✗ Entity not found: ${entityId}`);
        return null;
      }
    };

    // Helper to decode base64 target data
    // Binary format: [count(1)][target(14) * count]
    // Each target: id(1), x(2), y(2), z(2), velocity(2), snr(2), classifier(1), posture(1), active(1)
    const decodeTargetsBase64 = (base64String) => {
      if (!base64String || base64String === "") return [];

      try {
        // Decode base64 to binary
        const binaryString = atob(base64String);
        const bytes = new Uint8Array(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) {
          bytes[i] = binaryString.charCodeAt(i);
        }

        if (bytes.length < 1) return [];

        const count = bytes[0];
        const targets = [];

        // Parse big-endian int16 values
        const getInt16 = (offset) => {
          const val = (bytes[offset] << 8) | bytes[offset + 1];
          return val > 32767 ? val - 65536 : val;
        };

        for (let i = 0; i < count; i++) {
          const offset = 1 + (i * 14);
          if (offset + 14 > bytes.length) break;

          targets.push({
            id: bytes[offset],
            x: getInt16(offset + 1),
            y: getInt16(offset + 3),
            z: getInt16(offset + 5),
            velocity: getInt16(offset + 7),
            snr: getInt16(offset + 9),
            classifier: bytes[offset + 11],
            posture: bytes[offset + 12],
            active: bytes[offset + 13],
          });
        }

        console.log(`[FP2 Card] ✓ Decoded ${targets.length} targets from base64`);
        return targets;
      } catch (e) {
        console.error(`[FP2 Card] ✗ Base64 decode error:`, e.message);
        return [];
      }
    };

    // --- Static Map Config (from ESPHome service) ---
    // Use cached map config if available, otherwise use empty defaults
    const mapConfig = this.mapConfig || {};
    console.log(`[FP2 Card] Using cached map config:`, !!this.mapConfig);

    // Single decode path (D-02): every static grid overlay decodes through
    // FP2Codec.hexToGrid, which handles both the live 56-char and canonical
    // 80-char wire formats identically.
    const edgeLabelGrid = window.FP2Codec.hexToGrid(mapConfig.edge_grid);
    const entryExitGrid = window.FP2Codec.hexToGrid(mapConfig.exit_grid);
    const interferenceGrid = window.FP2Codec.hexToGrid(mapConfig.interference_grid);

    // mounting_position is returned by the firmware as a STRING enum (D-06) —
    // enum-validate against the three known values with a safe "wall" default.
    const KNOWN_MOUNTING_POSITIONS = ["wall", "left_upper_corner", "right_upper_corner"];
    // Honor the documented card-level `mounting_position` config as a fallback
    // when the device response omits it (WR-01). Placed before the whitelist
    // check so the override is enum-validated too; the "wall" default is kept.
    let mountingPosition = mapConfig.mounting_position || this.config.mounting_position;
    if (!KNOWN_MOUNTING_POSITIONS.includes(mountingPosition)) {
      if (mountingPosition !== undefined) {
        console.warn(`[FP2 Card] Unrecognized mounting_position "${mountingPosition}", falling back to "wall"`);
      }
      mountingPosition = "wall";
    }

    // left_right_reverse was never read into card state before this fix (GRID-02).
    const leftRightReverse = mapConfig.left_right_reverse === true;

    // --- Detection Zones (from map config with dynamic occupancy) ---
    const zones = [];
    if (mapConfig.zones && Array.isArray(mapConfig.zones)) {
      mapConfig.zones.forEach((zoneConfig, index) => {
        const zoneMap = window.FP2Codec.hexToGrid(zoneConfig.grid);

        // Look up occupancy state from the presence sensor entity
        let occupancy = false;
        if (zoneConfig.presence_sensor) {
          const presenceEntityId = `binary_sensor.${zoneConfig.presence_sensor}`;
          const presenceState = getEntityState(presenceEntityId);
          occupancy = presenceState === "on";
        }

        zones.push({
          // Human-facing label; must match the layer dropdown's 1-based
          // "Zone N" label for the SAME zone (WR-03). Real devices never emit
          // presence_sensor, so the "Zone N" fallback is the primary path.
          id: zoneConfig.presence_sensor || `Zone ${index + 1}`,
          map: zoneMap,
          occupancy: occupancy,
        });
        console.log(`[FP2 Card] ✓ Zone ${index}: presence_sensor=${zoneConfig.presence_sensor}, occupancy=${occupancy}`);
      });
    }
    console.log(`[FP2 Card] Total zones from config: ${zones.length}`);

    // --- Full Location Data Sensor ---
    // Text sensor containing base64-encoded binary target data
    // Binary format: [count(1)][target(14) * count]
    // Will be null/undefined when location reporting is disabled
    console.log(`[FP2 Card] Loading target data...`);
    const targetsEntity = this.config.targets_entity || `${prefix}_targets`;
    const targetsBase64 = getEntityState(targetsEntity);
    const targetData = targetsBase64 ? decodeTargetsBase64(targetsBase64) : [];
    const targetCount = targetData.length;
    console.log(`[FP2 Card] Total targets: ${targetCount}`);

    const result = {
      edgeLabelGrid,
      entryExitGrid,
      interferenceGrid,
      mountingPosition,
      leftRightReverse,
      zones,
      targets: targetData || [],
    };

    console.log(`[FP2 Card] ===== Entity gathering complete =====`);
    console.log(`[FP2 Card] Summary: ${zones.length} zones, ${targetCount} targets, mounting: ${mountingPosition}`);

    return result;
  }

  renderCanvas(data) {
    // Calculate canvas dimensions from card content container
    const containerWidth = this.content.clientWidth;
    if (!containerWidth || containerWidth === 0) {
      console.warn(`[FP2 Card] Container width is ${containerWidth}, deferring render`);
      return;
    }

    console.log(`[FP2 Card] Rendering canvas with container width: ${containerWidth}px`);
    const dpr = window.devicePixelRatio || 1;

    // Determine which cells to display
    let minX = 0,
      maxX = 13,
      minY = 0,
      maxY = 13;

    // CR-02: mirror the edge grid once, up front, so the "zoomed" crop-bounds
    // scan below and the displayData construction further down both read
    // the SAME (mirrored) grid. Computing crop bounds from the canonical
    // grid and then applying that window to the mirrored displayData grids
    // misaligns the zoomed view whenever left_right_reverse is true and the
    // edge grid is asymmetric.
    const mirroredEdgeLabelGrid = window.FP2Geometry.applyGridMirror(data.edgeLabelGrid, data.leftRightReverse);

    if (this.displayMode === "zoomed" && !this.editing) {
      // Calculate bounding box of non-edge cells
      let found = false;
      minX = 13;
      maxX = 0;
      minY = 13;
      maxY = 0;

      for (let y = 0; y < 14; y++) {
        for (let x = 0; x < 14; x++) {
          if (!mirroredEdgeLabelGrid[y][x]) {
            found = true;
            minX = Math.min(minX, x);
            maxX = Math.max(maxX, x);
            minY = Math.min(minY, y);
            maxY = Math.max(maxY, y);
          }
        }
      }

      if (!found) {
        // Fallback to full grid if no non-edge cells
        minX = 0;
        maxX = 13;
        minY = 0;
        maxY = 13;
      }
    }

    const gridWidth = maxX - minX + 1;
    const gridHeight = maxY - minY + 1;
    const cellSize = containerWidth / gridWidth;
    const canvasWidth = containerWidth;
    const canvasHeight = cellSize * gridHeight;

    // Set canvas size accounting for device pixel ratio
    this.canvas.width = canvasWidth * dpr;
    this.canvas.height = canvasHeight * dpr;
    this.canvas.style.width = `${canvasWidth}px`;
    this.canvas.style.height = `${canvasHeight}px`;

    this.ctx.scale(dpr, dpr);

    // Store for mouse interactions
    this.renderParams = {
      minX,
      maxX,
      minY,
      maxY,
      cellSize,
      canvasWidth,
      canvasHeight,
    };

    // Clear canvas
    this.ctx.clearRect(0, 0, canvasWidth, canvasHeight);

    // D-05/GEOM-02: single centralized display-time mirror. displayData is a
    // NEW object — data/editorState are never mutated in place. Every
    // grid-shaped field is passed through FP2Geometry.applyGridMirror, which
    // is the ONLY place left_right_reverse is checked for grid data; it
    // returns the input unchanged when leftRightReverse is false, so live
    // view (reverse off) stays byte-identical to before this change. This is
    // the sole point grids are mirrored — never re-check leftRightReverse
    // inside an individual draw* function (RESEARCH Pattern 4/Anti-Patterns).
    const displayData = {
      ...data,
      interferenceGrid: window.FP2Geometry.applyGridMirror(data.interferenceGrid, data.leftRightReverse),
      entryExitGrid: window.FP2Geometry.applyGridMirror(data.entryExitGrid, data.leftRightReverse),
      edgeLabelGrid: mirroredEdgeLabelGrid,
      zones: data.zones.map((zone) => ({
        ...zone,
        map: window.FP2Geometry.applyGridMirror(zone.map, data.leftRightReverse),
      })),
    };

    // Draw layers in order
    this.drawBaseGrid(displayData, minX, maxX, minY, maxY, cellSize);
    this.drawEdgeLabels(displayData, minX, maxX, minY, maxY, cellSize);
    this.drawInterferenceSources(displayData, minX, maxX, minY, maxY, cellSize);
    this.drawEntryExitZones(displayData, minX, maxX, minY, maxY, cellSize);
    this.drawDetectionZones(displayData, minX, maxX, minY, maxY, cellSize);
    this.drawSelectedLayerOutline(displayData, minX, maxX, minY, maxY, cellSize);
    this.drawHoverPreview(minX, maxX, minY, maxY, cellSize);
    this.drawTargets(displayData, minX, maxX, minY, maxY, cellSize);

    if (this.showSensorPosition) {
      // WR-01: pass displayData (not raw data) for consistency with every
      // other draw* call — drawSensorPosition mirrors sensorX itself, but
      // leftRightReverse must be read from the same object the rest of the
      // pipeline uses.
      this.drawSensorPosition(displayData, minX, maxX, minY, maxY, cellSize);
    }
  }

  drawBaseGrid(data, minX, maxX, minY, maxY, cellSize) {
    this.ctx.strokeStyle = "var(--divider-color)";
    this.ctx.lineWidth = 0.5;

    for (let y = minY; y <= maxY + 1; y++) {
      const yPos = (y - minY) * cellSize;
      this.ctx.beginPath();
      this.ctx.moveTo(0, yPos);
      this.ctx.lineTo((maxX - minX + 1) * cellSize, yPos);
      this.ctx.stroke();
    }

    for (let x = minX; x <= maxX + 1; x++) {
      const xPos = (x - minX) * cellSize;
      this.ctx.beginPath();
      this.ctx.moveTo(xPos, 0);
      this.ctx.lineTo(xPos, (maxY - minY + 1) * cellSize);
      this.ctx.stroke();
    }
  }

  drawEdgeLabels(data, minX, maxX, minY, maxY, cellSize) {
    this.ctx.fillStyle = "rgba(128, 128, 128, 0.5)";

    for (let y = minY; y <= maxY; y++) {
      for (let x = minX; x <= maxX; x++) {
        if (data.edgeLabelGrid[y][x]) {
          const xPos = (x - minX) * cellSize;
          const yPos = (y - minY) * cellSize;
          this.ctx.fillRect(xPos, yPos, cellSize, cellSize);

          // Add crosshatch pattern
          this.ctx.strokeStyle = "rgba(96, 96, 96, 0.3)";
          this.ctx.lineWidth = 1;
          this.ctx.beginPath();
          this.ctx.moveTo(xPos, yPos);
          this.ctx.lineTo(xPos + cellSize, yPos + cellSize);
          this.ctx.moveTo(xPos + cellSize, yPos);
          this.ctx.lineTo(xPos, yPos + cellSize);
          this.ctx.stroke();
        }
      }
    }
  }

  drawInterferenceSources(data, minX, maxX, minY, maxY, cellSize) {
    this.ctx.fillStyle = "rgba(255, 100, 100, 0.3)";

    for (let y = minY; y <= maxY; y++) {
      for (let x = minX; x <= maxX; x++) {
        if (data.interferenceGrid[y][x]) {
          const xPos = (x - minX) * cellSize;
          const yPos = (y - minY) * cellSize;
          this.ctx.fillRect(xPos, yPos, cellSize, cellSize);
        }
      }
    }
  }

  drawEntryExitZones(data, minX, maxX, minY, maxY, cellSize) {
    this.ctx.strokeStyle = "rgba(100, 200, 100, 0.8)";
    this.ctx.lineWidth = 3;

    for (let y = minY; y <= maxY; y++) {
      for (let x = minX; x <= maxX; x++) {
        if (data.entryExitGrid[y][x]) {
          const xPos = (x - minX) * cellSize;
          const yPos = (y - minY) * cellSize;
          this.ctx.strokeRect(xPos + 2, yPos + 2, cellSize - 4, cellSize - 4);
        }
      }
    }
  }

  drawDetectionZones(data, minX, maxX, minY, maxY, cellSize) {
    data.zones.forEach((zone) => {
      // Determine zone color based on occupancy
      const baseColor = zone.occupancy
        ? "rgba(100, 150, 255, 0.6)" // Occupied: bright blue
        : "rgba(100, 150, 255, 0.2)"; // Empty: light blue

      this.ctx.fillStyle = baseColor;

      // Fill zone cells
      for (let y = minY; y <= maxY; y++) {
        for (let x = minX; x <= maxX; x++) {
          if (zone.map[y][x]) {
            const xPos = (x - minX) * cellSize;
            const yPos = (y - minY) * cellSize;
            this.ctx.fillRect(xPos, yPos, cellSize, cellSize);
          }
        }
      }

      // Draw zone border
      this.ctx.strokeStyle = zone.occupancy
        ? "rgba(50, 100, 255, 0.8)"
        : "rgba(50, 100, 255, 0.4)";
      this.ctx.lineWidth = 2;

      // Find zone bounds for label placement
      let zoneMinX = 14,
        zoneMaxX = -1,
        zoneMinY = 14,
        zoneMaxY = -1;
      for (let y = 0; y < 14; y++) {
        for (let x = 0; x < 14; x++) {
          if (zone.map[y][x]) {
            zoneMinX = Math.min(zoneMinX, x);
            zoneMaxX = Math.max(zoneMaxX, x);
            zoneMinY = Math.min(zoneMinY, y);
            zoneMaxY = Math.max(zoneMaxY, y);
          }
        }
      }

      // Draw border around zone
      if (zoneMinX <= zoneMaxX) {
        for (let y = minY; y <= maxY; y++) {
          for (let x = minX; x <= maxX; x++) {
            if (zone.map[y][x]) {
              const xPos = (x - minX) * cellSize;
              const yPos = (y - minY) * cellSize;
              this.ctx.strokeRect(
                xPos + 1,
                yPos + 1,
                cellSize - 2,
                cellSize - 2,
              );
            }
          }
        }

        // Draw zone label if enabled and zone is visible
        if (
          this.showZoneLabels &&
          zoneMinX >= minX &&
          zoneMaxX <= maxX &&
          zoneMinY >= minY &&
          zoneMaxY <= maxY
        ) {
          const labelX = (zoneMinX + zoneMaxX) / 2;
          const labelY = (zoneMinY + zoneMaxY) / 2;
          const xPos = (labelX - minX) * cellSize + cellSize / 2;
          const yPos = (labelY - minY) * cellSize + cellSize / 2;

          this.ctx.fillStyle = "rgba(255, 255, 255, 0.9)";
          this.ctx.strokeStyle = "rgba(50, 100, 255, 0.8)";
          this.ctx.lineWidth = 2;

          const labelText = zone.id;
          this.ctx.font = `bold ${Math.min(cellSize * 0.6, 16)}px sans-serif`;
          this.ctx.textAlign = "center";
          this.ctx.textBaseline = "middle";

          const metrics = this.ctx.measureText(labelText);
          const padding = 4;
          this.ctx.fillRect(
            xPos - metrics.width / 2 - padding,
            yPos - cellSize * 0.3 - padding,
            metrics.width + padding * 2,
            cellSize * 0.6 + padding * 2,
          );

          this.ctx.fillStyle = "rgba(50, 100, 255, 1)";
          this.ctx.fillText(labelText, xPos, yPos);
        }
      }
    });
  }

  // UI-SPEC Note 3: dashed accent outline for this.selectedLayer's populated
  // cells, editing-only, drawn against the mirrored displayData grid so the
  // outline always lines up with what is actually painted on screen. Runs
  // after the four grid draw* calls and before drawTargets so the dashed
  // outline sits under the live overlay but over the filled cells.
  // ctx.save()/ctx.restore() bracket the sticky setLineDash/strokeStyle/
  // lineWidth state so it never leaks into later draw calls (RESEARCH
  // Pattern 3 gotcha).
  drawSelectedLayerOutline(data, minX, maxX, minY, maxY, cellSize) {
    if (!this.editing || !this.selectedLayer) return;

    let grid = null;
    if (this.selectedLayer === "interference") {
      grid = data.interferenceGrid;
    } else if (this.selectedLayer === "exit") {
      grid = data.entryExitGrid;
    } else if (this.selectedLayer === "edge") {
      grid = data.edgeLabelGrid;
    } else if (this.selectedLayer.startsWith("zone:new:")) {
      // CR-03 fix: a locally-added zone key (e.g. "zone:new:0") is not a
      // positional index into data.zones — parseInt("new:0", 10) is NaN,
      // which broke the outline entirely. Resolve from the already-mirrored
      // data.zones (tagged with its source `key` in updateCard()) instead
      // of reading raw editorState directly — every other branch here is
      // display-space (mirrored), and reading editorState would draw the
      // outline in canonical (unmirrored) space, misaligning it from the
      // fill whenever left_right_reverse is true.
      const match = Array.isArray(data.zones)
        ? data.zones.find((z) => z.key === this.selectedLayer)
        : undefined;
      grid = match ? match.map : null;
    } else if (this.selectedLayer.startsWith("zone:")) {
      const index = parseInt(this.selectedLayer.slice("zone:".length), 10);
      const zone = Array.isArray(data.zones) ? data.zones[index] : undefined;
      grid = zone ? zone.map : null;
    }

    if (!grid) return;

    this.ctx.save();
    this.ctx.setLineDash([4, 2]);
    this.ctx.strokeStyle = "var(--primary-color)";
    this.ctx.lineWidth = 1.5;

    for (let y = minY; y <= maxY; y++) {
      for (let x = minX; x <= maxX; x++) {
        if (grid[y][x]) {
          const xPos = (x - minX) * cellSize;
          const yPos = (y - minY) * cellSize;
          this.ctx.strokeRect(xPos, yPos, cellSize, cellSize);
        }
      }
    }

    this.ctx.restore();
  }

  // UI-SPEC Note 5: mouse-only, transient 30%-alpha hover-preview fill on
  // the single cell under the pointer, in the current paint-mode color.
  // Purely visual — never mutates editorState. Bounds-checked against the
  // currently-visible crop so a stale this._hoverCell from before a resize/
  // mode change never draws outside the grid. ctx.save()/restore() brackets
  // globalAlpha/fillStyle so this never leaks into later draw calls.
  drawHoverPreview(minX, maxX, minY, maxY, cellSize) {
    if (!this.editing || !this._hoverCell) return;

    const { x, y } = this._hoverCell;
    if (x < minX || x > maxX || y < minY || y > maxY) return;

    const xPos = (x - minX) * cellSize;
    const yPos = (y - minY) * cellSize;

    this.ctx.save();
    this.ctx.globalAlpha = 0.3;
    this.ctx.fillStyle =
      this.paintMode === "erase" ? "var(--error-color, #db4437)" : "var(--primary-color)";
    this.ctx.fillRect(xPos, yPos, cellSize, cellSize);
    this.ctx.restore();
  }

  drawTargets(data, minX, maxX, minY, maxY, cellSize) {
    if (!data.targets || !Array.isArray(data.targets)) return;

    data.targets.forEach((target) => {
      // D-06/GEOM-03: corner-mount (and wall-placeholder) transform lives in
      // FP2Geometry.targetToGridXY (single implementation, unit-tested by the
      // GEOM-03 harness invariant). D-05/GEOM-02: the SAME leftRightReverse
      // decision that mirrors the grids is applied here via
      // FP2Geometry.applyGridXMirror, immediately after the transform and
      // BEFORE the visible-range check, so the overlay never disagrees with
      // the painted grids (RESEARCH Pattern 4).
      const raw = window.FP2Geometry.targetToGridXY(target.x, target.y, data.mountingPosition);
      const gridX = window.FP2Geometry.applyGridXMirror(raw.gridX, data.leftRightReverse);
      const gridY = raw.gridY;

      const x = gridX;
      const y = gridY;

      // Check if target is in visible range
      if (x < minX || x > maxX + 1 || y < minY || y > maxY + 1) return;

      const xPos = (x - minX) * cellSize;
      const yPos = (y - minY) * cellSize;
      const radius = Math.min(cellSize * 0.3, 15);

      // UI-SPEC Note 4: white contrast halo, unconditionally (edit + live
      // view both), drawn immediately before the existing orange marker so
      // the target dot stays legible over any underlying fill color.
      this.ctx.strokeStyle = "rgba(255, 255, 255, 0.9)";
      this.ctx.lineWidth = 3;
      this.ctx.beginPath();
      this.ctx.arc(xPos, yPos, radius + 3, 0, Math.PI * 2);
      this.ctx.stroke();

      // Draw target circle
      this.ctx.fillStyle = "rgba(255, 200, 0, 0.8)";
      this.ctx.strokeStyle = "rgba(255, 150, 0, 1)";
      this.ctx.lineWidth = 2;

      this.ctx.beginPath();
      this.ctx.arc(xPos, yPos, radius, 0, Math.PI * 2);
      this.ctx.fill();
      this.ctx.stroke();

      // Draw target ID
      this.ctx.fillStyle = "rgba(0, 0, 0, 0.8)";
      this.ctx.font = `bold ${Math.min(cellSize * 0.4, 12)}px sans-serif`;
      this.ctx.textAlign = "center";
      this.ctx.textBaseline = "middle";
      this.ctx.fillText(target.id || "?", xPos, yPos);
    });
  }

  drawSensorPosition(data, minX, maxX, minY, maxY, cellSize) {
    let sensorX, sensorY;

    switch (data.mountingPosition) {
      case "left_upper_corner":
        // Top left corner of top-left cell (0,0)
        sensorX = 0;
        sensorY = 0;
        break;
      case "right_upper_corner":
        // Top right corner of top-right cell (13,0)
        sensorX = 14; // Right edge of cell 13
        sensorY = 0;
        break;
      case "wall":
      default:
        // Top edge between middle cells (between cells 6 and 7)
        sensorX = 7; // Grid line between cells 6 and 7
        sensorY = 0;
        break;
    }

    // WR-01: mirror the sensor's grid-space X the same way every other
    // coordinate on the canvas is mirrored (grids, zones, targets), so the
    // icon stays on the physically correct side when left_right_reverse is
    // true.
    sensorX = window.FP2Geometry.applyGridXMirror(sensorX, data.leftRightReverse);

    // Only draw if sensor is in visible range
    if (
      sensorX < minX - 1 ||
      sensorX > maxX + 1 ||
      sensorY < minY - 1 ||
      sensorY > maxY + 1
    ) {
      return;
    }

    const xPos = (sensorX - minX) * cellSize;
    const yPos = (sensorY - minY) * cellSize;
    const size = cellSize * 0.4;

    // Draw sensor icon (radar waves)
    this.ctx.strokeStyle = "rgba(255, 100, 100, 0.7)";
    this.ctx.lineWidth = 2;

    for (let i = 0; i < 3; i++) {
      this.ctx.beginPath();
      this.ctx.arc(xPos, yPos, size + i * 8, 0, Math.PI);
      this.ctx.stroke();
    }

    // Draw sensor dot
    this.ctx.fillStyle = "rgba(255, 50, 50, 0.9)";
    this.ctx.beginPath();
    this.ctx.arc(xPos, yPos, 4, 0, Math.PI * 2);
    this.ctx.fill();
  }

  updateInfoPanel(data) {
    const activeZones = data.zones.filter((z) => z.occupancy).length;
    const totalZones = data.zones.length;
    const targetCount = data.targets ? data.targets.length : 0;

    this.infoPanel.innerHTML = `
      <strong>Display Mode:</strong> ${this.editing ? "Editing (Full 14x14 Grid)" : (this.displayMode === "full" ? "Full Grid (14×14)" : "Zoomed View")} |
      <strong>Zones:</strong> ${activeZones}/${totalZones} occupied |
      <strong>Targets:</strong> ${targetCount}
      ${this.editing ? '<br>Tip: hold Shift or right-click while dragging to erase, no matter which mode is selected above.' : ""}
    `;
  }

  // PAINT-01/D-02, D-05: the single canvas->grid-cell implementation shared
  // by the pointer handlers (and by handleCanvasHover). WR-02: the stale
  // pre-refactor handleCanvasClick that duplicated this math (incorrectly,
  // without mirror-awareness) has been removed.
  // Re-reads this.renderParams and this._lastLeftRightReverse FRESH on every
  // call — never cache either across a drag (RESEARCH Security Domain,
  // T-03-T5): a resize or a leftRightReverse config change mid-drag must not
  // misalign already-in-flight painting. Returns the CANONICAL (unmirrored)
  // {x,y}, converting the displayed column back via
  // FP2Geometry.invertColumnMirror (D-05) so a click on the mirrored display
  // cell writes to the correct editorState index.
  canvasEventToGridCell(e) {
    const rect = this.canvas.getBoundingClientRect();
    // Pointer client coordinates are already CSS-pixel viewport values,
    // matching the CSS-pixel-space cellSize computed in renderCanvas — apply
    // no per-pixel-ratio scaling factor here (RESEARCH Pattern 3).
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const display = window.FP2Geometry.canvasPosToGridCell(x, y, this.renderParams);
    const canonicalX = window.FP2Geometry.invertColumnMirror(
      display.x,
      this._lastLeftRightReverse === true,
    );
    // CR-01: expose BOTH the canonical (post-invert) cell used for
    // editorState writes AND the raw display-space cell. Callers that draw
    // (e.g. the hover preview) must use displayX/displayY — the canvas is
    // always drawn in display/mirrored space via displayData, so drawing the
    // canonical x there would apply the mirror an odd number of times.
    return { x: canonicalX, y: display.y, displayX: display.x, displayY: display.y };
  }

  // ASVS V5/T-03-T4: bounds-guard every write to editorState — out-of-range
  // cell indices (e.g. a coalesced/late pointer event, or a position exactly
  // on the canvas border) are silently ignored, never thrown. PAINT-02/D-03:
  // erase is triggered by the paint/erase mode toggle OR a shift-modifier OR
  // a right-click, regardless of the toggle's current state.
  paintCell(cell, e) {
    if (!cell || cell.x < 0 || cell.x > 13 || cell.y < 0 || cell.y > 13) {
      return;
    }
    if (!this.selectedLayer || !this.editorState) return;
    const grid = this.editorState[this.selectedLayer];
    if (!Array.isArray(grid) || !Array.isArray(grid[cell.y])) return;

    const erase =
      this.paintMode === "erase" ||
      e.shiftKey === true ||
      e.button === 2 ||
      e.buttons === 2;
    grid[cell.y][cell.x] = erase ? 0 : 1;
  }

  handlePointerDown(e) {
    if (!this.editing || !this.renderParams) return;
    // WR-03: ignore a second concurrent pointer (e.g. an accidental extra
    // finger during a touch drag) while one is already painting — without
    // this guard, the second pointerdown would overwrite _lastCell/_painting
    // and cause subsequent pointermove events from EITHER pointer to
    // interpolate a spurious stroke between two unrelated touch points.
    if (this._painting) return;
    try {
      // MDN: keeps pointermove targeted at the canvas even if the drag
      // leaves its bounds. Guarded — a synthetic/test event or an already-
      // released pointerId can throw InvalidPointerId (never-throw
      // discipline, RESEARCH Pattern 1).
      this.canvas.setPointerCapture(e.pointerId);
    } catch (err) {
      console.warn(`[FP2 Card] setPointerCapture failed (pointerId ${e.pointerId}):`, err);
    }
    this._activePointerId = e.pointerId;
    this._painting = true;
    const cell = this.canvasEventToGridCell(e);
    this._lastCell = cell;
    this.paintCell(cell, e);
    this.updateCard();
  }

  handlePointerMove(e) {
    if (!this.editing) return;
    // WR-03: only the active pointer may continue the in-progress drag.
    if (this._painting && e.pointerId === this._activePointerId) {
      if (!this.renderParams) return;
      const cell = this.canvasEventToGridCell(e);
      // PAINT-01/D-02: interpolate every cell crossed since the last sample
      // so a fast drag never drops cells (RESEARCH Pattern 2).
      window.FP2Geometry.walkCellsBetween(this._lastCell, cell).forEach((c) =>
        this.paintCell(c, e),
      );
      this._lastCell = cell;
      this.updateCard();
    }
    // Hover-preview (mouse-only, Task 3/UI-SPEC Note 5) is handled by the
    // existing "mousemove"->handleCanvasHover listener above, not here —
    // pointermove and mousemove both fire for real mouse input and do
    // different, non-conflicting things (RESEARCH Pattern 1).
  }

  handlePointerUp(e) {
    // WR-03: an unrelated pointer lifting (e.g. the accidental second finger)
    // must not halt the active pointer's in-progress drag.
    if (e.pointerId !== this._activePointerId) return;
    this._painting = false;
    this._lastCell = null;
    this._activePointerId = null;
    // WR-04: releasing pointer capture is safe to attempt unconditionally —
    // the try/catch already handles the "not captured" case (e.g.
    // pointercancel, or a synthetic/test event). The previous guard on
    // this.renderParams had no logical connection to whether capture was
    // acquired (capture is set in handlePointerDown, itself gated on
    // this.renderParams at that EARLIER time) and would silently leak
    // capture if renderParams were ever reset to a falsy value.
    try {
      this.canvas.releasePointerCapture(e.pointerId);
    } catch (err) {
      // Not captured (e.g. pointercancel, or a synthetic/test event) —
      // safe to ignore (never-throw discipline).
    }
  }

  // UI-SPEC Note 5: mouse-only hover-preview. Reuses canvasEventToGridCell
  // (Task 1) so the preview cell obeys the same bounds/mirror logic as
  // painting. Redraws via the FULL updateCard()/renderCanvas() path — never
  // a hand-rolled lightweight redraw (RESEARCH Pitfall 7: a partial redraw
  // that skips the canvas.width reset accumulates ctx.scale).
  handleCanvasHover(e) {
    if (!this.editing || !this.renderParams) return;
    // CR-01: drawHoverPreview draws directly in display/mirrored space (like
    // the grids and drawSelectedLayerOutline), so store the display-space
    // cell here, NOT the canonical cell used for editorState writes.
    const cell = this.canvasEventToGridCell(e);
    this._hoverCell = { x: cell.displayX, y: cell.displayY };
    this.updateCard();
  }

  // Clears the hover-preview so no stale fill persists after the pointer
  // leaves the canvas (UI-SPEC Note 5).
  clearHoverPreview() {
    if (this._hoverCell) {
      this._hoverCell = null;
      this.updateCard();
    }
  }

  disconnectedCallback() {
    if (this.resizeObserver) {
      this.resizeObserver.disconnect();
    }
  }

  getCardSize() {
    return 4;
  }

  getGridOptions() {
    return {
      rows: 4,
      columns: 6,
      min_rows: 3,
      max_rows: 6,
    };
  }
}

customElements.define("aqara-fp2-card", AqaraFP2Card);

window.customCards = window.customCards || [];
window.customCards.push({
  type: "aqara-fp2-card",
  name: "Aqara FP2 Presence Sensor Card",
  description:
    "Visualizes Aqara FP2 presence sensor data with zones and target tracking",
  preview: true,
});
