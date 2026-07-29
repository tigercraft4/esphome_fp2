#pragma once
#include <cstdint>
#include <cstddef>

// PROGMEM is normally provided transitively by esphome/core/hal.h (empty on
// this project's ESP-IDF/ESP32 framework — flash and RAM share one address
// space, unlike AVR). Guarded fallback so this header also compiles in
// isolation (this plan's own `g++ -fsyntax-only` verification step) without
// redefining the real macro when included after ESPHome's core headers.
#ifndef PROGMEM
#define PROGMEM
#endif

// zones_page_bundle.h — flash-resident PROGMEM bundle for the /zones page
// (Phase 11 HTTP Transport Skeleton, WEBUI-01/03/05).
//
// Single self-contained HTML+CSS+JS byte array, no build step, no framework,
// no external font/icon/CDN. Served as-is via AsyncWebServerResponseProgmem
// by ZonesPageHandler (Plan 03). Uncompressed for this skeleton phase
// (11-RESEARCH.md Standard Stack "Alternatives Considered" A2).
//
// CSS uses literal hex tokens (11-UI-SPEC.md Color) rather than CSS custom
// properties: this page has no Home Assistant theme context, so no CSS
// custom-property references appear anywhere in this bundle.
static const char ZONES_PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>FP2 Zone Editor</title>
<style>
  * { box-sizing: border-box; }
  html, body {
    margin: 0;
    padding: 0;
    background: #F1F3F5;
    color: #1A1D21;
  }
  body {
    font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
    font-weight: 400;
    font-size: 16px;
    line-height: 1.5;
    padding: 16px;
    max-width: 640px;
    margin: 0 auto;
  }
  .header-bar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 8px;
    background: #FFFFFF;
    border: 1px solid #D8DCE1;
    border-radius: 8px;
    padding: 24px;
    margin-bottom: 32px;
  }
  .page-title {
    font-size: 28px;
    font-weight: 600;
    line-height: 1.2;
    margin: 0;
  }
  .badge {
    font-size: 14px;
    font-weight: 400;
    color: #5B6470;
    white-space: nowrap;
  }
  .badge.is-live { color: #2563EB; font-weight: 600; }
  .badge.is-connected { color: #0E7490; }
  .badge.is-disconnected { color: #DC2626; }
  .panel {
    background: #FFFFFF;
    border: 1px solid #D8DCE1;
    border-radius: 8px;
    padding: 24px;
    margin-bottom: 32px;
  }
  .panel-title {
    font-size: 20px;
    font-weight: 600;
    line-height: 1.2;
    margin: 0 0 16px 0;
  }
  .workspace {
    display: grid;
    grid-template-columns: 1fr;
    gap: 24px;
  }
  #live-grid-container {
    position: relative;
    background: #E8EBEE;
    border: 2px solid #D8DCE1;
    border-radius: 8px;
    box-shadow: inset 0 0 0 1px rgba(0, 0, 0, 0.04);
    line-height: 0;
    padding: 16px;
  }
  .corner-mark {
    position: absolute;
    width: 12px;
    height: 12px;
    pointer-events: none;
  }
  .corner-mark-tl { top: 6px; left: 6px; border-top: 2px solid #D8DCE1; border-left: 2px solid #D8DCE1; }
  .corner-mark-tr { top: 6px; right: 6px; border-top: 2px solid #D8DCE1; border-right: 2px solid #D8DCE1; }
  .corner-mark-bl { bottom: 6px; left: 6px; border-bottom: 2px solid #D8DCE1; border-left: 2px solid #D8DCE1; }
  .corner-mark-br { bottom: 6px; right: 6px; border-bottom: 2px solid #D8DCE1; border-right: 2px solid #D8DCE1; }
  #live-grid { display: block; width: 100%; height: auto; }
  #zone-list { max-height: 480px; overflow-y: auto; }
  .zone-card {
    border-bottom: 1px solid #D8DCE1;
    border-left: 4px solid transparent;
    padding: 16px 0 16px 12px;
    cursor: pointer;
  }
  .zone-card:last-child { border-bottom: none; }
  .zone-card.is-selected {
    border-radius: 4px;
  }
  .color-chip {
    display: inline-block;
    width: 12px;
    height: 12px;
    border-radius: 2px;
    flex-shrink: 0;
  }
  .zone-card-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 8px;
    flex-wrap: wrap;
  }
  .zone-name {
    font-size: 16px;
    font-weight: 400;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    max-width: 220px;
  }
  .zone-controls {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
  }
  .field-group {
    display: flex;
    align-items: center;
    gap: 4px;
  }
  .field-label {
    font-size: 14px;
    font-weight: 400;
    color: #5B6470;
  }
  select {
    font-family: inherit;
    font-size: 16px;
    padding: 4px 8px;
    border: 1px solid #D8DCE1;
    border-radius: 4px;
    background: #FFFFFF;
    color: #1A1D21;
    min-height: 44px;
  }
  select:focus, button:focus {
    outline: 2px solid #2563EB;
    outline-offset: 2px;
  }
  .save-btn {
    font-family: inherit;
    font-size: 16px;
    font-weight: 600;
    color: #FFFFFF;
    background: #2563EB;
    border: none;
    border-radius: 4px;
    padding: 0 16px;
    min-width: 44px;
    min-height: 44px;
    cursor: pointer;
  }
  .save-btn:disabled {
    background: #D8DCE1;
    color: #5B6470;
    cursor: not-allowed;
  }
  .status-line {
    font-size: 14px;
    font-weight: 400;
    color: #5B6470;
    margin-top: 8px;
  }
  .status-line.is-failed { color: #DC2626; }
  .empty-state, .loading-state, .error-state {
    font-size: 16px;
    font-weight: 400;
    color: #5B6470;
  }
  .empty-state h3, .error-state h3 {
    font-size: 16px;
    font-weight: 600;
    color: #1A1D21;
    margin: 0 0 8px 0;
  }
  .reload-btn {
    margin-top: 16px;
    font-family: inherit;
    font-size: 16px;
    font-weight: 600;
    color: #FFFFFF;
    background: #2563EB;
    border: none;
    border-radius: 4px;
    padding: 0 16px;
    min-width: 44px;
    min-height: 44px;
    cursor: pointer;
  }
  .remove-btn {
    font-family: inherit;
    font-size: 16px;
    font-weight: 600;
    color: #DC2626;
    background: #FFFFFF;
    border: 1px solid #DC2626;
    border-radius: 4px;
    padding: 0 16px;
    min-width: 44px;
    min-height: 44px;
    cursor: pointer;
  }
  .remove-btn:disabled {
    background: #D8DCE1;
    color: #5B6470;
    border-color: #D8DCE1;
    cursor: not-allowed;
  }
  .layer-toolbar {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
    margin-bottom: 16px;
    padding-bottom: 16px;
    border-bottom: 1px solid #D8DCE1;
  }
  .mode-toggle-btn {
    font-family: inherit;
    font-size: 16px;
    font-weight: 600;
    color: #FFFFFF;
    border: none;
    border-radius: 4px;
    padding: 0 16px;
    min-width: 44px;
    min-height: 44px;
    cursor: pointer;
  }
  .mode-toggle-btn.mode-paint { background: #2563EB; }
  .mode-toggle-btn.mode-erase { background: #DC2626; }
  .clear-layer-btn {
    font-family: inherit;
    font-size: 16px;
    font-weight: 600;
    color: #DC2626;
    background: #FFFFFF;
    border: 1px solid #DC2626;
    border-radius: 4px;
    padding: 0 16px;
    min-width: 44px;
    min-height: 44px;
    cursor: pointer;
  }
  .legend-row {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
    margin-top: 16px;
  }
  .legend-entry {
    display: flex;
    align-items: center;
    gap: 4px;
  }
  .legend-swatch {
    display: inline-block;
    width: 12px;
    height: 12px;
    border-radius: 2px;
    flex-shrink: 0;
  }
  .global-zone-field {
    margin-top: 16px;
  }
  .add-zone-bar {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
    margin-bottom: 16px;
    padding-bottom: 16px;
    border-bottom: 1px solid #D8DCE1;
  }
  .add-zone-bar .save-btn {
    background: #16A34A;
  }
  .add-zone-bar .save-btn:disabled {
    background: #D8DCE1;
    color: #5B6470;
  }
  .capacity-message {
    font-size: 14px;
    font-weight: 400;
    color: #5B6470;
    margin-bottom: 16px;
    padding-bottom: 16px;
    border-bottom: 1px solid #D8DCE1;
  }
  .export-import-bar {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
    margin-bottom: 16px;
  }
  .export-textarea {
    width: 100%;
    min-height: 160px;
    font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
    font-size: 14px;
    font-weight: 400;
    line-height: 1.4;
    padding: 8px;
    border: 1px solid #D8DCE1;
    border-radius: 4px;
    background: #F1F3F5;
    color: #1A1D21;
    resize: vertical;
    margin-bottom: 8px;
  }
  .export-caption {
    margin-bottom: 8px;
  }
  .editing-indicator {
    font-size: 14px;
    font-weight: 400;
    color: #5B6470;
    margin-bottom: 8px;
  }
  .sidebar-caption {
    font-size: 14px;
    font-weight: 400;
    color: #5B6470;
    margin: 0 0 16px 0;
  }
  @media (min-width: 900px) {
    body { max-width: 1080px; }
    .workspace { grid-template-columns: 1fr 320px; }
    .sidebar {
      position: sticky;
      top: 16px;
      max-height: calc(100vh - 32px);
      overflow-y: auto;
    }
  }
  @media (max-width: 480px) {
    body { padding: 8px; }
    .header-bar, .panel { padding: 16px; }
    .zone-name { max-width: 140px; }
  }
</style>
</head>
<body>
  <header class="header-bar">
    <h1 class="page-title">FP2 Zone Editor</h1>
    <span id="live-badge" class="badge">Connecting&hellip;</span>
  </header>

  <div class="workspace">
  <section class="panel" id="live-view-panel">
    <h2 class="panel-title">Painting</h2>
    <div id="editing-indicator" class="editing-indicator"></div>
    <div class="layer-toolbar">
      <div class="field-group">
        <label class="field-label" for="layer-select">Layer</label>
        <select id="layer-select" disabled>
          <option value="" disabled selected>Loading&hellip;</option>
        </select>
      </div>
      <button type="button" id="paint-mode-toggle" class="mode-toggle-btn mode-paint">Paint</button>
      <button type="button" id="clear-layer-btn" class="clear-layer-btn">Clear Layer</button>
    </div>
    <div id="live-grid-container">
      <div class="corner-mark corner-mark-tl"></div>
      <div class="corner-mark corner-mark-tr"></div>
      <div class="corner-mark corner-mark-bl"></div>
      <div class="corner-mark corner-mark-br"></div>
      <svg id="live-grid" viewBox="0 0 14 14" preserveAspectRatio="xMidYMid meet"></svg>
    </div>
    <div class="legend-row">
      <span class="legend-entry"><span class="legend-swatch" style="background: rgba(220, 38, 38, 0.30);"></span>Interference</span>
      <span class="legend-entry"><span class="legend-swatch" style="background: rgba(22, 163, 74, 0.75);"></span>Exit</span>
      <span class="legend-entry"><span class="legend-swatch" style="background: rgba(91, 100, 112, 0.35);"></span>Edge</span>
    </div>
    <div class="global-zone-field field-group">
      <label class="field-label" for="global-zone-select">Global Zone</label>
      <select id="global-zone-select">
        <option value="">-- not set --</option>
        <option value="low">Low</option>
        <option value="medium">Medium</option>
        <option value="high">High</option>
      </select>
    </div>
  </section>

  <section class="panel sidebar" id="zones-panel">
    <h2 class="panel-title">Zones</h2>
    <p class="sidebar-caption">Each zone's color on the grid above matches its card below.</p>
    <div id="add-zone-bar" class="add-zone-bar">
      <div class="field-group">
        <label class="field-label" for="add-zone-select">Add Zone</label>
        <select id="add-zone-select"></select>
      </div>
      <button type="button" id="add-zone-btn" class="save-btn">Add</button>
    </div>
    <div id="capacity-message" class="capacity-message" style="display:none;">All 32 zone slots in use &mdash; remove one to add another</div>
    <div id="zone-list">
      <div class="loading-state">Loading zones&hellip;</div>
    </div>
  </section>
  </div>

  <!-- Export / Import bar (13-05-PLAN.md, WEBUI-04): page-level actions, not
       per-zone, same placement as card.js's single global toolbar. No new
       panel-title heading is introduced here (13-UI-SPEC.md Typography: "no
       new heading-level text this phase beyond" the Painting rename) - this
       reuses the .panel container's visual styling only. Both buttons start
       disabled and are enabled once loadZones() first resolves successfully
       (13-UI-SPEC.md UI Considerations: "Export and Import buttons stay
       disabled until the first successful load so neither can act on
       stale/absent data"). -->
  <section class="panel" id="export-import-panel">
    <div class="export-import-bar">
      <button type="button" id="export-yaml-btn" class="save-btn" disabled>Export YAML</button>
      <button type="button" id="import-device-btn" class="save-btn" disabled>Import from Device</button>
    </div>
    <textarea id="export-textarea" class="export-textarea" readonly style="display:none;"></textarea>
    <div id="export-caption" class="field-label export-caption" style="display:none;">Paste below your existing aqara_fp2: configuration</div>
    <div id="export-status" class="status-line"></div>
    <div id="import-status" class="status-line"></div>
  </section>

<script>
(function () {
  'use strict';

  // Ported verbatim from card.js's ZONE_TYPES_JS mapping (lines ~280-291),
  // itself ported from ZONE_TYPES in components/aqara_fp2/__init__.py.
  var ZONE_TYPES_JS = {
    none: 0, tv: 2, green_plant: 10, leisure: 11, dressing: 13,
    closet: 14, desk: 15, shower: 23, stairs: 36
  };
  // Mirrors SENSITIVITY_INT_TO_STRING_JS (card.js ~line 305): 1/2/3 are the
  // protocol's raw sensitivity ints, sent to save_zone_to_sensor() as-is.
  var SENSITIVITY_LABELS = { 1: 'Low', 2: 'Medium', 3: 'High' };
  // Export-only: maps the same raw ints to the lowercase enum strings
  // CONFIG_SCHEMA's SENSITIVITY_LEVELS actually expects in YAML
  // (components/aqara_fp2/__init__.py) - distinct from SENSITIVITY_LABELS
  // above, which is Title Case for on-screen display only.
  var SENSITIVITY_INT_TO_STRING = { 1: 'low', 2: 'medium', 3: 'high' };
  var GRID_SIZE = 14;
  var SVG_NS = 'http://www.w3.org/2000/svg';

  var badgeEl = document.getElementById('live-badge');
  var zoneListEl = document.getElementById('zone-list');
  var liveGridEl = document.getElementById('live-grid');
  var addZoneBarEl = document.getElementById('add-zone-bar');
  var addZoneSelectEl = document.getElementById('add-zone-select');
  var addZoneBtnEl = document.getElementById('add-zone-btn');
  var capacityMessageEl = document.getElementById('capacity-message');
  var layerSelectEl = document.getElementById('layer-select');
  var paintModeToggleEl = document.getElementById('paint-mode-toggle');
  var clearLayerBtnEl = document.getElementById('clear-layer-btn');
  var globalZoneSelectEl = document.getElementById('global-zone-select');
  var exportYamlBtnEl = document.getElementById('export-yaml-btn');
  var exportTextareaEl = document.getElementById('export-textarea');
  var exportCaptionEl = document.getElementById('export-caption');
  var exportStatusEl = document.getElementById('export-status');
  var importDeviceBtnEl = document.getElementById('import-device-btn');
  var importStatusEl = document.getElementById('import-status');

  // Pitfall 1 / RESEARCH A1: the firmware's save-confirmation state
  // (save_pending()/save_ok()/save_error()) is one shared, global set of
  // fields, not keyed by zone_id. While ANY save is in flight, disable ALL
  // rows' Save buttons (not just the clicked one) so a second click can't
  // clobber the first save's in-flight confirmation state.
  var savePending = false;

  // Populated from GET /api/zones's top-level fields (json_get_map_data()
  // already emits these) so the live-overlay target transform matches
  // card.js's FP2Geometry mirror/mount handling instead of assuming "wall".
  var mountingPosition = 'wall';
  var leftRightReverse = false;

  function escapeHtml(value) {
    var div = document.createElement('div');
    div.textContent = value == null ? '' : String(value);
    return div.innerHTML;
  }

  // === FP2Codec START (ported verbatim from card.js lines 25-121) ===
  // Byte-exact port of parse_ascii_grid/grid_to_hex_string
  // (components/aqara_fp2/__init__.py). Pure functions only — no DOM
  // dependency, drops into this closure unchanged (13-PATTERNS.md "FP2Codec
  // port"). gridToHex ALWAYS emits the 80-char/40-byte canonical write
  // format; hexToGrid accepts BOTH the 56-char (14-row card/display format,
  // GET /api/zones' hex fields) and 80-char (write) input, and never throws
  // on malformed input (WR-02/WR-03 never-throw idiom, Pitfall 4 in
  // 13-RESEARCH.md: never round-trip a GET /api/zones grid string straight
  // into a POST /api/zones/save body without going through hexToGrid/
  // gridToHex first).
  var FP2Codec = (function () {
    var ROWS_OUT = 20; // protocol grid rows (only first 14 are ever populated; offset_row=0)
    var OFFSET_ROW = 0;
    var OFFSET_COL = 2;

    function emptyGrid() {
      var g = [];
      for (var r = 0; r < GRID_SIZE; r++) g.push(Array(GRID_SIZE).fill(0));
      return g;
    }

    function asciiToGrid(ascii) {
      var lines = (ascii || '')
        .trim()
        .split('\n')
        .map(function (l) { return l.replace(/ /g, ''); })
        .filter(function (l) { return l.length > 0; });
      if (lines.length !== GRID_SIZE) {
        console.warn('[FP2 Zones] asciiToGrid: expected ' + GRID_SIZE + ' rows, got ' + lines.length + ', using empty grid');
        return emptyGrid();
      }
      var grid = [];
      for (var r = 0; r < GRID_SIZE; r++) {
        if (lines[r].length !== GRID_SIZE) {
          console.warn('[FP2 Zones] asciiToGrid: row ' + (r + 1) + ' must have ' + GRID_SIZE + ' chars, got ' + lines[r].length + ', using empty grid');
          return emptyGrid();
        }
        grid.push(Array.from(lines[r]).map(function (ch) { return (ch === 'x' || ch === 'X') ? 1 : 0; }));
      }
      return grid;
    }

    function gridToAscii(grid) {
      return grid.map(function (row) {
        return row.map(function (v) { return v ? 'X' : '.'; }).join('');
      }).join('\n');
    }

    function gridToHex(grid) {
      // grid: 14x14 array of 0/1 -> full 40-byte / 80-hex-char protocol blob.
      var bytes = new Uint8Array(ROWS_OUT * 2);
      for (var r = 0; r < GRID_SIZE; r++) {
        var outR = r + OFFSET_ROW;
        var rowVal = 0;
        for (var c = 0; c < GRID_SIZE; c++) {
          if (grid[r][c]) {
            var outC = c + OFFSET_COL;
            rowVal |= 1 << (15 - outC); // MSB-first: col 0 -> bit 15
          }
        }
        bytes[outR * 2] = (rowVal >> 8) & 0xff; // high byte (Big-Endian)
        bytes[outR * 2 + 1] = rowVal & 0xff; // low byte
      }
      return Array.from(bytes).map(function (b) { return b.toString(16).padStart(2, '0'); }).join('');
    }

    function hexToGrid(hex) {
      // Accepts either the 80-char (20-row) canonical protocol blob or the
      // live GET /api/zones response's 56-char (14-row) card format.
      // Defensive-parse-never-throw: malformed input warns and returns a
      // 14x14 zero grid, it never throws. A legitimately-absent optional
      // grid (interference/exit/edge unconfigured) is the normal empty-grid
      // case — return silently to avoid a console.warn on every render
      // cycle (WR-03). Reserve warnings for genuinely present-but-malformed
      // non-empty input.
      if (hex == null || hex === '') {
        return emptyGrid();
      }
      if (typeof hex !== 'string' || hex.length % 4 !== 0) {
        console.warn('[FP2 Zones] hexToGrid: invalid/malformed hex (length ' + (typeof hex === 'string' ? hex.length : typeof hex) + '), using empty grid');
        return emptyGrid();
      }
      // Validate the hex alphabet before parsing — parseInt stops at the
      // first non-hex char and would silently mis-decode a corrupt-but-
      // correct-length payload (WR-02).
      if (!/^[0-9a-fA-F]*$/.test(hex)) {
        console.warn('[FP2 Zones] hexToGrid: non-hex characters in input, using empty grid');
        return emptyGrid();
      }
      if (hex.length !== 56 && hex.length !== 80) {
        console.warn('[FP2 Zones] hexToGrid: unexpected hex length ' + hex.length + ' (expected 56 or 80), decoding first ' + GRID_SIZE + ' rows anyway');
      }
      var availableRows = hex.length / 4;
      var rowsToRead = Math.min(GRID_SIZE, availableRows);
      var grid = emptyGrid();
      for (var r = 0; r < rowsToRead; r++) {
        var rowVal = parseInt(hex.substr(r * 4, 4), 16);
        for (var c = 0; c < GRID_SIZE; c++) {
          var outC = c + OFFSET_COL;
          grid[r][c] = (rowVal >> (15 - outC)) & 1;
        }
      }
      return grid;
    }

    return { asciiToGrid: asciiToGrid, gridToAscii: gridToAscii, gridToHex: gridToHex, hexToGrid: hexToGrid };
  })();
  // === FP2Codec END ===

  // === FP2Geometry START (ported from card.js lines 133-166, 216-251) ===
  // Pure mirror/cell-walk math. Single-check-point discipline (13-CONTEXT.md,
  // 13-PATTERNS.md "FP2Geometry port"): applyGridMirror is THE one place
  // leftRightReverse is checked for grid data; invertColumnMirror is its own
  // inverse for a display column; mirrorGrid MUST NOT mutate its input (it
  // may alias editorState). walkCellsBetween is the Bresenham/DDA drag-paint
  // interpolation walk (never-throw on non-finite input, returns [endpoint]
  // on a null anchor).
  var FP2Geometry = (function () {
    function mirrorColumn(col) {
      // Discrete grid column mirror: col -> 13-col. Its own exact inverse.
      return GRID_SIZE - 1 - col;
    }

    function mirrorGrid(grid) {
      // Returns a NEW 14x14 array with each row reversed. MUST NOT mutate
      // the input: the input may alias editorState, so always slice()
      // before reverse().
      if (!Array.isArray(grid)) {
        console.warn('[FP2 Zones] mirrorGrid: expected an array grid, returning input unchanged');
        return grid;
      }
      return grid.map(function (row) { return Array.isArray(row) ? row.slice().reverse() : row; });
    }

    function applyGridMirror(grid, reverse) {
      // The single place leftRightReverse is checked for grid data.
      return reverse ? mirrorGrid(grid) : grid;
    }

    function mirrorGridX(gridX) {
      // Continuous 0..14 mirror for the live target overlay (not a discrete
      // column index).
      return GRID_SIZE - gridX;
    }

    function applyGridXMirror(gridX, reverse) {
      // The single place leftRightReverse is checked for the live target
      // overlay's continuous X coordinate.
      return reverse ? mirrorGridX(gridX) : gridX;
    }

    function invertColumnMirror(displayCol, reverse) {
      // Exact inverse used by the pointer/click path: the column mirror is
      // its own inverse.
      return reverse ? mirrorColumn(displayCol) : displayCol;
    }

    function walkCellsBetween(a, b) {
      // Integer Bresenham/DDA walk returning every {x,y} cell from a to b
      // inclusive, 8-connected (no diagonal gaps) — drag-paint interpolation.
      if (a == null) {
        return [b];
      }
      if (b == null) {
        return [a];
      }
      if (!isFinite(a.x) || !isFinite(a.y) || !isFinite(b.x) || !isFinite(b.y)) {
        console.warn('[FP2 Zones] walkCellsBetween: non-finite coordinate, returning endpoint only');
        return [b];
      }
      var x0 = Math.round(a.x);
      var y0 = Math.round(a.y);
      var x1 = Math.round(b.x);
      var y1 = Math.round(b.y);
      var cells = [];
      var x = x0;
      var y = y0;
      var dx = Math.abs(x1 - x0);
      var dy = -Math.abs(y1 - y0);
      var sx = x0 < x1 ? 1 : -1;
      var sy = y0 < y1 ? 1 : -1;
      var err = dx + dy;
      // eslint-disable-next-line no-constant-condition
      while (true) {
        cells.push({ x: x, y: y });
        if (x === x1 && y === y1) break;
        var e2 = 2 * err;
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
      mirrorColumn: mirrorColumn,
      mirrorGrid: mirrorGrid,
      applyGridMirror: applyGridMirror,
      mirrorGridX: mirrorGridX,
      applyGridXMirror: applyGridXMirror,
      invertColumnMirror: invertColumnMirror,
      walkCellsBetween: walkCellsBetween
    };
  })();
  // === FP2Geometry END ===

  // --- Live-overlay grid: static lines drawn once; target dots per frame ---

  function buildGridLines() {
    for (var i = 0; i <= GRID_SIZE; i++) {
      var vLine = document.createElementNS(SVG_NS, 'line');
      vLine.setAttribute('x1', i);
      vLine.setAttribute('y1', 0);
      vLine.setAttribute('x2', i);
      vLine.setAttribute('y2', GRID_SIZE);
      vLine.setAttribute('stroke', '#D8DCE1');
      vLine.setAttribute('stroke-width', '0.03');
      liveGridEl.appendChild(vLine);

      var hLine = document.createElementNS(SVG_NS, 'line');
      hLine.setAttribute('x1', 0);
      hLine.setAttribute('y1', i);
      hLine.setAttribute('x2', GRID_SIZE);
      hLine.setAttribute('y2', i);
      hLine.setAttribute('stroke', '#D8DCE1');
      hLine.setAttribute('stroke-width', '0.03');
      liveGridEl.appendChild(hLine);
    }
  }
  buildGridLines();

  var targetDotsGroup = document.createElementNS(SVG_NS, 'g');
  targetDotsGroup.setAttribute('id', 'target-dots');
  liveGridEl.appendChild(targetDotsGroup);

  // === Paint layers (13-03-PLAN.md Task 2, WEBUI-04/D-01) ===
  //
  // editorState: client-side only, lives in this closure. Keyed
  // 'interference' / 'exit' / 'edge' / 'zone:<id>', mirroring card.js's
  // shape (13-RESEARCH.md Pattern 1). Seeded from the SAME GET /api/zones
  // response loadZones() already awaits below — no new endpoint.
  var editorState = {};

  // Plan 04 owns the Layer toolbar UI (<select> + Paint/Erase toggle) that
  // will read/write this closure var; default to the first fixed layer so
  // the selected-layer outline and Task 3's paint routing are testable in
  // isolation before that UI exists.
  var selectedLayer = 'interference';

  // Five new SVG groups, created ONCE at load and inserted via
  // insertBefore(group, targetDotsGroup) so each lands immediately before
  // targetDotsGroup — repeating this against the same reference node builds
  // the exact documented back-to-front stacking order without ever
  // reordering targetDotsGroup itself (13-PATTERNS.md "SVG z-order and
  // sparse redraw"): grid lines -> edge -> interference -> exit -> zones ->
  // selected-layer outline -> targets (always on top, unchanged).
  function makeLayerGroup(id) {
    var g = document.createElementNS(SVG_NS, 'g');
    g.setAttribute('id', id);
    liveGridEl.insertBefore(g, targetDotsGroup);
    return g;
  }
  var edgeLayerGroup = makeLayerGroup('edge-layer');
  var interferenceLayerGroup = makeLayerGroup('interference-layer');
  var exitLayerGroup = makeLayerGroup('exit-layer');
  var zonesLayerGroup = makeLayerGroup('zones-layer');
  var selectedOutlineGroup = makeLayerGroup('selected-layer-outline');

  // Painting Layer Colors, 13-UI-SPEC.md Color table — the only place these
  // literal values are declared.
  var ZONE_FILL = 'rgba(37, 99, 235, 0.35)';
  var ZONE_BORDER = 'rgba(37, 99, 235, 0.7)';
  var INTERFERENCE_FILL = 'rgba(220, 38, 38, 0.30)';
  var EXIT_STROKE = 'rgba(22, 163, 74, 0.75)';
  var EDGE_FILL = 'rgba(91, 100, 112, 0.35)';
  var EDGE_HATCH_STROKE = 'rgba(26, 29, 33, 0.3)';
  var SELECTED_OUTLINE_STROKE = '#2563EB';

  function clearGroup(group) {
    while (group.firstChild) {
      group.removeChild(group.firstChild);
    }
  }

  // Walks a canonical (write-space) grid, mirrors it through the single
  // FP2Geometry.applyGridMirror check point into display space, and invokes
  // cellDrawFn(group, x, y) for every active display-space cell. Grid may
  // be null/undefined (an as-yet-unseeded layer) — no-op, not an error.
  function drawCellsInto(group, grid, cellDrawFn) {
    if (!grid) return;
    var displayGrid = FP2Geometry.applyGridMirror(grid, leftRightReverse);
    for (var y = 0; y < GRID_SIZE; y++) {
      var row = displayGrid[y];
      if (!row) continue;
      for (var x = 0; x < GRID_SIZE; x++) {
        if (row[x]) cellDrawFn(group, x, y);
      }
    }
  }

  function makeFillCellDrawer(fill, border) {
    return function (group, x, y) {
      var rect = document.createElementNS(SVG_NS, 'rect');
      rect.setAttribute('x', x);
      rect.setAttribute('y', y);
      rect.setAttribute('width', 1);
      rect.setAttribute('height', 1);
      rect.setAttribute('fill', fill);
      if (border) {
        rect.setAttribute('stroke', border);
        rect.setAttribute('stroke-width', '2');
        // Keeps the border a crisp N-CSS-pixel line regardless of the SVG's
        // viewBox-to-rendered-size scale (the grid is fluid-width per
        // 13-UI-SPEC.md, unlike the fixed-pixel <canvas> card.js drew on).
        rect.setAttribute('vector-effect', 'non-scaling-stroke');
      } else {
        rect.setAttribute('stroke', 'none');
      }
      group.appendChild(rect);
    };
  }

  function makeStrokeCellDrawer(stroke, strokeWidth, dash) {
    return function (group, x, y) {
      var rect = document.createElementNS(SVG_NS, 'rect');
      rect.setAttribute('x', x);
      rect.setAttribute('y', y);
      rect.setAttribute('width', 1);
      rect.setAttribute('height', 1);
      rect.setAttribute('fill', 'none');
      rect.setAttribute('stroke', stroke);
      rect.setAttribute('stroke-width', strokeWidth);
      rect.setAttribute('vector-effect', 'non-scaling-stroke');
      if (dash) rect.setAttribute('stroke-dasharray', dash);
      group.appendChild(rect);
    };
  }

  function makeCrosshatchCellDrawer(fill, hatchStroke) {
    var fillDrawer = makeFillCellDrawer(fill, null);
    return function (group, x, y) {
      fillDrawer(group, x, y);
      var l1 = document.createElementNS(SVG_NS, 'line');
      l1.setAttribute('x1', x);
      l1.setAttribute('y1', y);
      l1.setAttribute('x2', x + 1);
      l1.setAttribute('y2', y + 1);
      l1.setAttribute('stroke', hatchStroke);
      l1.setAttribute('stroke-width', '1');
      l1.setAttribute('vector-effect', 'non-scaling-stroke');
      group.appendChild(l1);
      var l2 = document.createElementNS(SVG_NS, 'line');
      l2.setAttribute('x1', x + 1);
      l2.setAttribute('y1', y);
      l2.setAttribute('x2', x);
      l2.setAttribute('y2', y + 1);
      l2.setAttribute('stroke', hatchStroke);
      l2.setAttribute('stroke-width', '1');
      l2.setAttribute('vector-effect', 'non-scaling-stroke');
      group.appendChild(l2);
    };
  }

  // Per-layer sparse redraw (13-PATTERNS.md Pattern 4): each function
  // clears and rebuilds only its own group. Called on initial load
  // (loadZones()) and, later, on a paint/erase mutation of that specific
  // layer (Task 3) — never on the ~1/s SSE target_update tick, which only
  // touches targetDotsGroup via renderTargets() above.
  function redrawEdgeLayer() {
    clearGroup(edgeLayerGroup);
    drawCellsInto(edgeLayerGroup, editorState.edge, makeCrosshatchCellDrawer(EDGE_FILL, EDGE_HATCH_STROKE));
  }

  function redrawInterferenceLayer() {
    clearGroup(interferenceLayerGroup);
    drawCellsInto(interferenceLayerGroup, editorState.interference, makeFillCellDrawer(INTERFERENCE_FILL, null));
  }

  function redrawExitLayer() {
    clearGroup(exitLayerGroup);
    drawCellsInto(exitLayerGroup, editorState.exit, makeStrokeCellDrawer(EXIT_STROKE, '3'));
  }

  // All zones render simultaneously into the SAME shared group (parity with
  // card.js showing every zone at once, not just the selected one) — this
  // one group is rebuilt in full whenever ANY zone's grid changes, which is
  // still "sparse" relative to the SSE tick it must never run on.
  function redrawZonesLayer() {
    clearGroup(zonesLayerGroup);
    var drawer = makeFillCellDrawer(ZONE_FILL, ZONE_BORDER);
    for (var key in editorState) {
      if (!Object.prototype.hasOwnProperty.call(editorState, key)) continue;
      if (key.indexOf('zone:') !== 0) continue;
      drawCellsInto(zonesLayerGroup, editorState[key], drawer);
    }
  }

  // Dashed accent outline around the currently-selected layer's populated
  // cells only (editing indicator, ported from card.js's
  // drawSelectedLayerOutline, dash pattern [4,2] verbatim).
  function redrawSelectedOutline() {
    clearGroup(selectedOutlineGroup);
    var grid = selectedLayer ? editorState[selectedLayer] : null;
    if (!grid) return;
    drawCellsInto(selectedOutlineGroup, grid, makeStrokeCellDrawer(SELECTED_OUTLINE_STROKE, '2', '4,2'));
  }

  function redrawAllLayers() {
    redrawEdgeLayer();
    redrawInterferenceLayer();
    redrawExitLayer();
    redrawZonesLayer();
    redrawSelectedOutline();
  }
  // === Paint layers END ===

  // === Pointer-driven paint/erase (13-03-PLAN.md Task 3, WEBUI-04/D-01) ===
  //
  // Plan 04 owns the actual Layer-select/Paint-Erase-toggle UI; paintMode is
  // the shared closure var it will flip between 'paint'/'erase'. Default
  // 'paint' (alongside Task 2's default selectedLayer = 'interference') so
  // this task's pointer logic is testable in isolation before that UI
  // exists.
  var paintMode = 'paint';

  var painting = false;
  var activePointerId = null;
  var lastStrokeCell = null; // canonical (write-space) {x,y}; null between strokes

  // Maps a layerKey to the one owning sparse-redraw function (Task 2). All
  // zones share the single zones-layer group, so any 'zone:<id>' key routes
  // to the same redrawZonesLayer(). Also refreshes the selected-layer
  // outline when the mutated layer IS the currently-selected one — still a
  // single targeted group, not a full-canvas rebuild, and keeps the outline
  // from going stale mid-stroke.
  function redrawLayerByKey(layerKey) {
    if (layerKey === 'interference') redrawInterferenceLayer();
    else if (layerKey === 'exit') redrawExitLayer();
    else if (layerKey === 'edge') redrawEdgeLayer();
    else if (layerKey.indexOf('zone:') === 0) redrawZonesLayer();
    if (layerKey === selectedLayer) redrawSelectedOutline();
  }

  // SVG-native pointer-to-cell math (13-RESEARCH.md Pattern 2): viewBox="0 0
  // 14 14" means 1 SVG user unit == 1 grid cell, so no cellSize/minX/minY
  // bookkeeping is needed at all (unlike card.js's canvas renderParams).
  // Never throws — returns null on an unlaid-out element (null CTM) or an
  // out-of-bounds pick (ASVS V5 bounds guard).
  function clientToGridCell(svg, clientX, clientY) {
    var pt = svg.createSVGPoint();
    pt.x = clientX;
    pt.y = clientY;
    var ctm = svg.getScreenCTM();
    if (!ctm) return null;
    var loc = pt.matrixTransform(ctm.inverse());
    var x = Math.floor(loc.x);
    var y = Math.floor(loc.y);
    if (x < 0 || x > 13 || y < 0 || y > 13) return null;
    return { x: x, y: y };
  }

  // ASVS V5 / T-13-03: bounds-guard AGAIN at the mutation site (defense in
  // depth, ported from card.js's paintCell guard) — never trust a computed
  // cell index without a range check, even one already checked upstream by
  // clientToGridCell.
  function paintCell(layerKey, x, y, erase) {
    if (x < 0 || x > 13 || y < 0 || y > 13) return;
    var grid = editorState[layerKey];
    if (!Array.isArray(grid) || !Array.isArray(grid[y])) return;
    grid[y][x] = erase ? 0 : 1;
    redrawLayerByKey(layerKey);
  }

  // Ported behavior from card.js's canvasEventToGridCell/paintCell (not its
  // canvas-pixel math, per 13-RESEARCH.md Pattern 2): pick the display-space
  // cell, invert the column mirror to get the canonical write-space x (the
  // single mirror check point on the write side), then interpolate every
  // intermediate cell since the last sample via FP2Geometry.walkCellsBetween
  // so a fast drag never skips cells. Erase is live per-cell (matches
  // card.js): the Paint/Erase toggle OR a held Shift, checked at the time of
  // each sample, not locked in at pointerdown.
  function strokeToClientPoint(clientX, clientY, shiftKey) {
    var displayCell = clientToGridCell(liveGridEl, clientX, clientY);
    if (!displayCell) return;
    var canonicalCell = {
      x: FP2Geometry.invertColumnMirror(displayCell.x, leftRightReverse),
      y: displayCell.y
    };
    var erase = paintMode === 'erase' || shiftKey === true;
    FP2Geometry.walkCellsBetween(lastStrokeCell, canonicalCell).forEach(function (cell) {
      paintCell(selectedLayer, cell.x, cell.y, erase);
    });
    lastStrokeCell = canonicalCell;
  }

  function endStroke(e) {
    if (e.pointerId !== activePointerId) return;
    painting = false;
    activePointerId = null;
    lastStrokeCell = null;
    try {
      liveGridEl.releasePointerCapture(e.pointerId);
    } catch (err) {
      // Not captured (e.g. pointercancel, or a synthetic/test event) — safe
      // to ignore (never-throw discipline).
    }
  }

  // Pointer Events (not separate mouse/touch handlers) so the same code
  // path drives mouse, touch, and pen (13-UI-SPEC.md — this page is
  // reachable from a phone on the LAN). card.js's right-click-to-erase is
  // deliberately NOT ported here — no touch equivalent exists and the
  // Paint/Erase toggle already covers the same need (13-UI-SPEC.md).
  liveGridEl.addEventListener('pointerdown', function (e) {
    // WR-03 precedent (card.js): ignore a second concurrent pointer (e.g.
    // an accidental extra finger during a touch drag) while one is already
    // painting, so a stray pointermove from either pointer can't interpolate
    // a spurious stroke between two unrelated touch points.
    if (painting) return;
    try {
      // Keeps pointermove targeted at #live-grid even if the drag leaves
      // its bounds. Guarded — a synthetic/test event or an already-released
      // pointerId can throw (never-throw discipline).
      liveGridEl.setPointerCapture(e.pointerId);
    } catch (err) {
      console.warn('[FP2 Zones] setPointerCapture failed (pointerId ' + e.pointerId + '):', err);
    }
    activePointerId = e.pointerId;
    painting = true;
    lastStrokeCell = null;
    strokeToClientPoint(e.clientX, e.clientY, e.shiftKey);
  });

  liveGridEl.addEventListener('pointermove', function (e) {
    if (!painting || e.pointerId !== activePointerId) return;
    strokeToClientPoint(e.clientX, e.clientY, e.shiftKey);
  });

  liveGridEl.addEventListener('pointerup', endStroke);
  liveGridEl.addEventListener('pointercancel', endStroke);
  liveGridEl.addEventListener('pointerleave', endStroke);
  // === Pointer-driven paint/erase END ===

  // === Layer toolbar (13-04-PLAN.md Task 1, WEBUI-04/D-01) ===
  //
  // Ports card.js's .editor-controls row (layer-select/paint-mode-toggle/
  // clear-layer-btn) minus icons (13-UI-SPEC.md "Layer toolbar" — no
  // <ha-icon> custom element on this standalone page). Drives the
  // selectedLayer/paintMode closure vars Task 2/3 above already default and
  // read.

  // (Re)builds the #layer-select <option>s in the FIXED order Interference/
  // Exit/Edge, then one "Zone {id}" per zone, from the SAME zones array
  // loadZones() already fetched for the Zone List panel (same source, same
  // order) — no separate fetch. Called once loadZones()'s GET /api/zones
  // resolves; until then the select stays disabled showing the "Loading…"
  // placeholder already in the initial markup.
  function populateLayerSelect(zones) {
    var fixedLayers = [
      ['interference', 'Interference Grid'],
      ['exit', 'Exit Grid'],
      ['edge', 'Edge Grid']
    ];
    // Same id/label fallback as renderZoneRow() above (zone.presence_sensor
    // || 'Zone {id}') so the Layer select and the Zone List panel never
    // disagree on a zone's display label.
    var zoneLayers = (zones || []).map(function (z) {
      return ['zone:' + z.id, z.presence_sensor ? z.presence_sensor : ('Zone ' + z.id)];
    });
    var desired = fixedLayers.concat(zoneLayers);

    var previousValue = selectedLayer;

    layerSelectEl.disabled = false;
    while (layerSelectEl.firstChild) {
      layerSelectEl.removeChild(layerSelectEl.firstChild);
    }
    desired.forEach(function (pair) {
      var option = document.createElement('option');
      option.value = pair[0];
      option.textContent = pair[1];
      layerSelectEl.appendChild(option);
    });

    // Preserve the current selection across a rebuild (e.g. after Add/
    // Remove triggers a fresh loadZones()) if it still exists; otherwise
    // fall back to whatever the <select> now defaults to (its first option).
    if (previousValue && desired.some(function (pair) { return pair[0] === previousValue; })) {
      layerSelectEl.value = previousValue;
      selectedLayer = previousValue;
    } else {
      selectedLayer = layerSelectEl.value || null;
    }

    // Programmatically setting .value does not fire 'change' — reconcile the
    // dashed selected-layer outline explicitly (mirrors card.js's
    // populateLayerSelect()/clearSelectedLayer() reconcile discipline).
    redrawSelectedOutline();
  }

  // Selecting a "Zone N" paint layer visually links the painting panel to
  // that zone's row-level "Save to Sensor" button (paint->save discoverability).
  function highlightZoneRowForLayer(layerKey) {
    var stale = zoneListEl.querySelectorAll('.zone-row.is-paint-target');
    for (var i = 0; i < stale.length; i++) {
      stale[i].classList.remove('is-paint-target');
    }
    if (!layerKey || layerKey.indexOf('zone:') !== 0) return;
    var zoneId = layerKey.slice('zone:'.length);
    var row = zoneListEl.querySelector('.zone-row[data-zone-id="' + zoneId + '"]');
    if (!row) return;
    row.classList.add('is-paint-target');
    row.scrollIntoView({ behavior: 'smooth', block: 'center' });
  }

  layerSelectEl.addEventListener('change', function () {
    selectedLayer = layerSelectEl.value || null;
    console.log('[FP2 Zones] Layer selection changed: ' + selectedLayer);
    redrawSelectedOutline();
    highlightZoneRowForLayer(selectedLayer);
  });

  // Paint/Erase mode toggle: default "Paint" (Accent), active "Erase"
  // (Destructive) — exact tooltip copy from 13-UI-SPEC.md's Copywriting
  // Contract, ported verbatim.
  var PAINT_ERASE_TOOLTIP = 'Click/drag to fill cells on the selected layer. Hold Shift while painting to erase instead.';
  paintModeToggleEl.title = PAINT_ERASE_TOOLTIP;

  function updatePaintModeAffordance() {
    var isErase = paintMode === 'erase';
    paintModeToggleEl.classList.toggle('mode-paint', !isErase);
    paintModeToggleEl.classList.toggle('mode-erase', isErase);
    paintModeToggleEl.textContent = isErase ? 'Erase' : 'Paint';
  }
  updatePaintModeAffordance();

  paintModeToggleEl.addEventListener('click', function () {
    paintMode = paintMode === 'erase' ? 'paint' : 'erase';
    console.log('[FP2 Zones] Paint mode toggled: ' + paintMode);
    updatePaintModeAffordance();
  });

  // Clear Layer: window.confirm-gated (exact copy from 13-UI-SPEC.md, ported
  // verbatim from card.js's clearSelectedLayer()), empties ONLY the
  // currently-selected layer's grid in editorState. Cancel is a no-op.
  clearLayerBtnEl.addEventListener('click', function () {
    if (!selectedLayer) {
      console.warn('[FP2 Zones] Clear blocked: no layer selected');
      return;
    }
    var label = (layerSelectEl.selectedIndex >= 0 && layerSelectEl.options[layerSelectEl.selectedIndex])
      ? layerSelectEl.options[layerSelectEl.selectedIndex].textContent
      : selectedLayer;
    if (!window.confirm('Clear "' + label + '"? This cannot be undone.')) {
      return;
    }
    // Array.from(...) builds 14 independent row arrays — never share a
    // single row reference across the grid (mirrors card.js's
    // clearSelectedLayer() / FP2Codec.emptyGrid() discipline).
    editorState[selectedLayer] = Array.from({ length: GRID_SIZE }, function () {
      return Array(GRID_SIZE).fill(0);
    });
    console.log('[FP2 Zones] Cleared layer: ' + selectedLayer);
    redrawLayerByKey(selectedLayer);
  });
  // === Layer toolbar END ===

  // === Global Zone field (13-04-PLAN.md Task 2, WEBUI-04/D-01) ===
  //
  // Ported from card.js's .global-zone-field, copy unchanged ("-- not set --"
  // / "Low" / "Medium" / "High"). Stored in this closure var so a future
  // Export (Plan 05) can emit it and Import can reset it — this plan only
  // wires the field's own state, not export/import.
  var globalZoneSensitivity = null;
  globalZoneSelectEl.addEventListener('change', function () {
    globalZoneSensitivity = globalZoneSelectEl.value === '' ? null : globalZoneSelectEl.value;
    console.log('[FP2 Zones] Global Zone presence_sensitivity changed: ' + globalZoneSensitivity);
  });
  // === Global Zone field END ===

  // === Export YAML / Import from Device (13-05-PLAN.md, WEBUI-04) ===
  //
  // Adapted from card.js's validateGridsForExport/buildExportYaml/
  // copyToClipboardWithFallback/handleExportClick/handleImportClick/
  // mergeImportedMapConfig (card.js lines 681-1015). The LOGIC (suspicious-
  // grid gate, only-set-optional-keys builder, injection-safe ids, 3-tier
  // clipboard fallback, confirm-then-act ordering, D-05/D-07 reset-on-
  // import) is preserved. The STATE SOURCE differs from card.js: this
  // bundle has no zoneMeta closure object (card.js's from-scratch per-
  // session drafting state) - sensitivity/zone_type are read live from
  // each zone row's <select> elements instead, since this is a device-
  // hosted CURRENT-STATE editor (every zone already exists on the device),
  // not a blank drafting canvas. There is also no "zone:new:*" locally-
  // drafted zone concept in this bundle (Add Zone always POSTs immediately
  // via /api/zones/create and only becomes a real "zone:<id>" editorState
  // key after a successful create+poll) - the 'zone:new:' guards below are
  // kept anyway so that invariant holds by construction, not merely by the
  // accidental absence of a code path that could create one.

  var ZONE_TYPE_NAMES_BY_VALUE = (function () {
    var out = {};
    for (var name in ZONE_TYPES_JS) {
      if (Object.prototype.hasOwnProperty.call(ZONE_TYPES_JS, name)) {
        out[ZONE_TYPES_JS[name]] = name;
      }
    }
    return out;
  })();

  // Returns { presenceSensitivity (raw int), zoneType (raw int), label }
  // read live from the zone's DOM row, or defensive defaults if the row
  // can't be found (should be unreachable - loadZones() always populates
  // editorState's zone:<id> keys in lockstep with the Zone List panel's
  // rows).
  function getZoneRowMeta(zoneId) {
    var row = zoneListEl.querySelector('.zone-row[data-zone-id="' + zoneId + '"]');
    if (!row) {
      return { presenceSensitivity: 2, zoneType: 0, label: 'Zone ' + zoneId };
    }
    var sensitivitySelect = row.querySelector('.sensitivity-select');
    var zoneTypeSelect = row.querySelector('.zone-type-select');
    var nameEl = row.querySelector('.zone-name');
    return {
      presenceSensitivity: sensitivitySelect ? parseInt(sensitivitySelect.value, 10) : 2,
      zoneType: zoneTypeSelect ? parseInt(zoneTypeSelect.value, 10) : 0,
      label: nameEl ? nameEl.textContent : ('Zone ' + zoneId)
    };
  }

  function hasActiveCell(grid) {
    return Array.isArray(grid) && grid.some(function (row) {
      return Array.isArray(row) && row.some(function (cell) { return !!cell; });
    });
  }

  function emitGridLines(grid, keyName, indentLevel, out) {
    var pad = function (level) { return new Array(level + 1).join('  '); };
    out.push(pad(indentLevel) + keyName + ': |');
    FP2Codec.gridToAscii(grid).split('\n').forEach(function (row) {
      out.push(pad(indentLevel + 1) + row);
    });
  }

  // Only-set-optional-keys YAML builder, byte-exact to parse_ascii_grid
  // (grids are serialized via FP2Codec.gridToAscii, the same codec module
  // FP2Codec.gridToHex/hexToGrid already ported verbatim - never a second
  // grid-to-text implementation). presence_sensitivity is always emitted;
  // zone_type is only emitted when the row's current selection is not the
  // "none"/0 default - the closest equivalent in this data model to
  // card.js's "meta.zoneType !== null" only-set check, since this bundle's
  // <select> always shows SOME value (there is no separate "untouched"
  // state to test here). D-06 (13-CONTEXT.md, 13-RESEARCH.md Pitfall 3):
  // motion_timeout has NO UI control this phase - the emission point is
  // documented below for parity-gap traceability but can never fire.
  function buildExportYaml() {
    var lines = [];
    var usedIds = {};
    function uniqueId(candidate) {
      var id = candidate;
      var n = 2;
      while (usedIds[id]) {
        id = candidate + '_' + n++;
      }
      if (id !== candidate) {
        console.warn('[FP2 Zones] buildExportYaml: zone id "' + candidate + '" collided - renamed to "' + id + '"');
      }
      usedIds[id] = true;
      return id;
    }

    [['interference', 'interference_grid'], ['exit', 'exit_grid'], ['edge', 'edge_grid']].forEach(function (pair) {
      var grid = editorState[pair[0]];
      if (!Array.isArray(grid)) {
        console.warn('[FP2 Zones] buildExportYaml: malformed grid: ' + pair[0]);
        return;
      }
      if (hasActiveCell(grid)) emitGridLines(grid, pair[1], 0, lines);
    });

    // Only-if-touched global_zone: block (card.js Phase 5/D-03 precedent),
    // matching CONFIG_SCHEMA's own key order (global_zone precedes zones
    // in components/aqara_fp2/__init__.py).
    if (globalZoneSensitivity !== null) {
      lines.push('global_zone:');
      lines.push('  presence_sensitivity: ' + globalZoneSensitivity);
    }

    var zoneKeys = Object.keys(editorState).filter(function (k) { return k.indexOf('zone:') === 0; });
    if (zoneKeys.length > 0) {
      lines.push('zones:');
      zoneKeys.forEach(function (key) {
        var grid = editorState[key];
        var zoneId = key.slice('zone:'.length);
        var meta = getZoneRowMeta(zoneId);
        // Zone ids in this bundle are always the device's own numeric
        // zone_id (never user-supplied text), so a plain "zone_<id>" is
        // already injection-safe by construction - no slugify step needed
        // (unlike card.js's resolveZoneExportId, which slugifies a
        // user-editable zone name).
        lines.push('  - id: ' + uniqueId('zone_' + zoneId));
        if (Array.isArray(grid)) emitGridLines(grid, 'grid', 2, lines);
        else console.warn('[FP2 Zones] buildExportYaml: malformed grid: ' + key);
        lines.push('    presence_sensitivity: ' + (SENSITIVITY_INT_TO_STRING[meta.presenceSensitivity] || 'medium'));
        var tn = meta.zoneType !== 0 ? ZONE_TYPE_NAMES_BY_VALUE[meta.zoneType] : null;
        if (tn) lines.push('    zone_type: ' + tn);
        // D-06: motion_timeout has no UI control this phase - there is no
        // value to read here, so the key is always omitted. This is the
        // SAME known export-parity gap v1.0 had, not a silent drop
        // introduced this phase (13-CONTEXT.md D-06).
      });
    }

    return lines.join('\n');
  }

  // Flags a grid that's entirely empty (0 active cells) OR entirely filled
  // (all 196 active) as suspicious; a normal partially-painted grid
  // produces no entry. A malformed (non-14x14) grid is flagged defensively
  // instead of counted - should be unreachable given editorState is always
  // seeded 14x14 by FP2Codec.hexToGrid. Never throws (card.js VAL-01/D-06
  // precedent).
  function validateGridsForExport() {
    var suspicious = [];
    var globalLayers = [
      ['interference', 'Interference Grid'],
      ['exit', 'Exit Grid'],
      ['edge', 'Edge Grid']
    ];
    var zoneLayers = Object.keys(editorState)
      .filter(function (k) { return k.indexOf('zone:') === 0; })
      .map(function (k) { return [k, getZoneRowMeta(k.slice('zone:'.length)).label]; });

    globalLayers.concat(zoneLayers).forEach(function (pair) {
      var key = pair[0];
      var label = pair[1];
      var grid = editorState[key];
      var malformed = !Array.isArray(grid) || grid.length !== GRID_SIZE ||
        !grid.every(function (row) { return Array.isArray(row) && row.length === GRID_SIZE; });
      if (malformed) {
        suspicious.push(label + ': malformed grid (not 14x14)');
        return;
      }
      var active = grid.reduce(function (sum, row) {
        return sum + row.reduce(function (s, c) { return s + (c ? 1 : 0); }, 0);
      }, 0);
      if (active === 0) {
        suspicious.push(label + ': empty (no cells painted)');
      } else if (active === GRID_SIZE * GRID_SIZE) {
        suspicious.push(label + ': entirely filled (all 196 cells active)');
      }
    });

    return suspicious;
  }

  // 3-tier clipboard fallback, never throws (every fallback only warns).
  // Tier 1: Clipboard API, requires a secure context - this device serves
  // plain http://, so tier 1 may be entirely unavailable
  // (13-RESEARCH.md "Don't Hand-Roll"). Tier 2: document.execCommand('copy'),
  // honoring its BOOLEAN return. Tier 3: leave the pre-selected textarea
  // visible for a manual copy. The textarea stays populated/visible
  // regardless of which tier "succeeded" - the caller shows it before this
  // runs. Returns a Promise resolving to 'clipboard-api' | 'exec-command' |
  // 'manual-only'.
  function copyToClipboardWithFallback(text, textareaEl) {
    function execCommandFallback() {
      if (textareaEl && document.execCommand) {
        try {
          textareaEl.focus();
          textareaEl.select();
          if (typeof textareaEl.setSelectionRange === 'function') {
            textareaEl.setSelectionRange(0, text.length);
          }
          if (document.execCommand('copy') === true) {
            return 'exec-command';
          }
          console.warn("[FP2 Zones] copyToClipboardWithFallback: execCommand('copy') returned false, falling back to manual copy");
        } catch (e) {
          console.warn('[FP2 Zones] copyToClipboardWithFallback: execCommand threw, falling back to manual copy', e);
        }
      }
      console.warn('[FP2 Zones] copyToClipboardWithFallback: automatic copy unavailable - textarea left visible/pre-selected for manual copy');
      return 'manual-only';
    }

    if (window.isSecureContext && navigator.clipboard && navigator.clipboard.writeText) {
      return navigator.clipboard.writeText(text).then(function () {
        return 'clipboard-api';
      }, function (e) {
        console.warn('[FP2 Zones] copyToClipboardWithFallback: Clipboard API write failed, falling back', e);
        return execCommandFallback();
      });
    }
    return Promise.resolve(execCommandFallback());
  }

  function setExportStatus(text, isFailed) {
    exportStatusEl.classList.toggle('is-failed', !!isFailed);
    exportStatusEl.textContent = text || '';
  }

  // Confirm-then-act ordering (card.js Pitfall 9 precedent):
  // validateGridsForExport() + a synchronous window.confirm() resolve to
  // completion BEFORE buildExportYaml()/the clipboard write ever run -
  // cancelling the confirm is an unconditional early return, nothing is
  // ever built or copied.
  function handleExportClick() {
    var suspicious = validateGridsForExport();
    if (suspicious.length > 0) {
      var proceed = window.confirm('The following grids look suspicious:\n\n' + suspicious.join('\n') + '\n\nExport anyway?');
      if (!proceed) {
        return;
      }
    }

    var yaml = buildExportYaml();
    exportTextareaEl.value = yaml;
    exportTextareaEl.style.display = '';
    exportCaptionEl.style.display = '';
    setExportStatus('');

    copyToClipboardWithFallback(yaml, exportTextareaEl).then(function (tier) {
      if (tier === 'manual-only') {
        setExportStatus('Clipboard unavailable — copy the YAML below manually.', false);
      } else {
        setExportStatus('Copied to clipboard.', false);
      }
    });
  }

  exportYamlBtnEl.addEventListener('click', handleExportClick);

  var IMPORT_CONFIRM_COPY = "Import will overwrite the global grids and every existing device zone's grid/sensitivity with the device's current configuration. zone_type, motion_timeout, and Global Zone sensitivity will be reset (the device can't report these). Locally-added zones are kept, and zones already created on this device are included. Continue?";
  var IMPORT_FAILURE_COPY = "Import failed — could not fetch the device's current configuration. Try again.";

  function setImportStatus(text, isFailed) {
    importStatusEl.classList.toggle('is-failed', !!isFailed);
    importStatusEl.textContent = text || '';
  }

  // Merges a fresh GET /api/zones response INTO editorState (never a
  // wholesale replace of unrelated state) - mirrors card.js's
  // mergeImportedMapConfig(). Per-row deep copy: FP2Codec.hexToGrid always
  // builds brand-new row arrays (never aliases editorState), so no
  // additional .slice() step is needed here, unlike card.js's explicit
  // `.map(function (row) { return row.slice(); })` (which exists there
  // because gatherEntityData() can return aliased rows - this bundle's
  // hexToGrid never does).
  //
  // CRITICAL (Pitfall 2 / D-05 / D-07, 13-RESEARCH.md): GET /api/zones DOES
  // report zone.zone_type per zone whenever the device has one set, but
  // this merge DELIBERATELY DISCARDS it on every zone/import to honor the
  // locked D-05/D-07 decision - this is intentional, NOT an oversight. Do
  // NOT "fix" this by reading z.zone_type into the rendered rows below. The
  // same applies to motion_timeout (no live source at all, same as v1.0)
  // and Global Zone sensitivity (also has no live source). The reset is
  // surfaced to the user via the exact inline Import-success copy in
  // handleImportClick() below, so the UI never silently claims to
  // round-trip a field it actually discards (13-05-PLAN.md prohibition).
  function mergeImportedMapConfig(data) {
    mountingPosition = data.mounting_position || mountingPosition;
    leftRightReverse = data.left_right_reverse === true;

    editorState.interference = FP2Codec.hexToGrid(data.interference_grid);
    editorState.exit = FP2Codec.hexToGrid(data.exit_grid);
    editorState.edge = FP2Codec.hexToGrid(data.edge_grid);

    var deviceZones = Array.isArray(data.zones) ? data.zones : [];

    // Orphan deletion: drop every zone:<id> key not present in the freshly-
    // imported list (a zone-count DECREASE since the last load). The
    // 'zone:new:' guard preserves any locally-added-but-not-yet-saved key
    // by construction (this bundle never actually creates one - Add Zone
    // always POSTs immediately - but the guard costs nothing and matches
    // card.js's Pitfall 1 discipline exactly).
    var deviceZoneKeys = {};
    deviceZones.forEach(function (z) { deviceZoneKeys['zone:' + z.id] = true; });
    Object.keys(editorState).forEach(function (key) {
      if (key.indexOf('zone:') === 0 && key.indexOf('zone:new:') !== 0 && !deviceZoneKeys[key]) {
        delete editorState[key];
      }
    });

    deviceZones.forEach(function (z) {
      editorState['zone:' + z.id] = FP2Codec.hexToGrid(z.grid);
    });

    // Reset zone_type/motion_timeout/Global Zone on EVERY import (Pitfall
    // 2/D-05/D-07 - see function header comment above). Build a rendering
    // copy of the zones array with zone_type stripped so renderZoneRow()
    // (which defaults an absent/non-number zone_type to 0/"none") shows the
    // reset state, never the device's real reported value. sensitivity IS
    // recoverable and is passed through unchanged - only zone_type is
    // deliberately discarded.
    var resetZonesForRender = deviceZones.map(function (z) {
      return { id: z.id, sensitivity: z.sensitivity, presence_sensor: z.presence_sensor };
    });
    renderZoneList(resetZonesForRender);
    redrawAllLayers();
    populateLayerSelect(resetZonesForRender);

    globalZoneSensitivity = null;
    globalZoneSelectEl.value = '';

    return deviceZones.length;
  }

  // Confirm -> await fetch('/api/zones') (SAME endpoint the page-load seed
  // and the Zone List panel already use - NO new endpoint) -> merge ->
  // inline success/failure copy, strictly in that order. A cancelled
  // confirm is an unconditional early return BEFORE any side effect
  // (mirrors handleExportClick()'s guard-then-act shape, extended to the
  // async case). A fetch failure leaves editorState untouched entirely -
  // the merge only ever runs after a successful fetch.
  function handleImportClick() {
    if (!window.confirm(IMPORT_CONFIRM_COPY)) {
      return;
    }

    importDeviceBtnEl.disabled = true;
    importDeviceBtnEl.textContent = 'Importing…';
    setImportStatus('');

    fetch('/api/zones')
      .then(function (resp) {
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        return resp.json();
      })
      .then(function (data) {
        var importedCount = mergeImportedMapConfig(data);
        importDeviceBtnEl.disabled = false;
        importDeviceBtnEl.textContent = 'Import from Device';
        setImportStatus(
          'Imported ' + importedCount + " zone(s) from the device. zone_type, motion_timeout, and Global Zone sensitivity can't be read from the device and were reset — re-set them if needed before exporting.",
          false
        );
      })
      .catch(function () {
        importDeviceBtnEl.disabled = false;
        importDeviceBtnEl.textContent = 'Import from Device';
        setImportStatus(IMPORT_FAILURE_COPY, true);
      });
  }

  importDeviceBtnEl.addEventListener('click', handleImportClick);
  // === Export YAML / Import from Device END ===

  // Ported from card.js's FP2Geometry.targetToGridXY (lines ~173-188): corner
  // mounts use the verified 7m x 7m transform; wall mount reuses card.js's
  // own not-yet-verified placeholder (rawX/rawY * 0.01) rather than inventing
  // a new one here.
  function targetToGridXY(rawX, rawY, mp) {
    if (!isFinite(rawX) || !isFinite(rawY)) return { gridX: 0, gridY: 0 };
    if (mp === 'left_upper_corner' || mp === 'right_upper_corner') {
      return { gridX: ((-rawX + 400) / 800.0) * 14.0, gridY: (rawY / 800.0) * 14.0 };
    }
    return { gridX: rawX * 0.01, gridY: rawY * 0.01 };
  }

  // Ported verbatim from card.js:2017-2062 (decodeTargetsBase64).
  // Binary format: [count(1)][target(14) * count]. Each target, big-endian:
  // id(1), x(2), y(2), z(2), velocity(2), snr(2), classifier(1), posture(1), active(1).
  function decodeTargetsBase64(base64String) {
    if (!base64String || base64String === '') return [];
    try {
      var binaryString = atob(base64String);
      var bytes = new Uint8Array(binaryString.length);
      for (var i = 0; i < binaryString.length; i++) {
        bytes[i] = binaryString.charCodeAt(i);
      }
      if (bytes.length < 1) return [];
      var count = bytes[0];
      var targets = [];
      var getInt16 = function (offset) {
        var val = (bytes[offset] << 8) | bytes[offset + 1];
        return val > 32767 ? val - 65536 : val;
      };
      for (var t = 0; t < count; t++) {
        var offset = 1 + t * 14;
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
          active: bytes[offset + 13]
        });
      }
      return targets;
    } catch (e) {
      console.error('[FP2 Zones] base64 decode error:', e.message);
      return [];
    }
  }

  function renderTargets(targets) {
    while (targetDotsGroup.firstChild) {
      targetDotsGroup.removeChild(targetDotsGroup.firstChild);
    }
    for (var i = 0; i < targets.length; i++) {
      var tgt = targets[i];
      if (!tgt.active) continue;
      var xy = targetToGridXY(tgt.x, tgt.y, mountingPosition);
      // Refactored to route through the single ported FP2Geometry mirror
      // check point (13-03-PLAN.md Task 1) — same behavior as the prior
      // local applyGridXMirror duplicate, just one call target now.
      var gx = FP2Geometry.applyGridXMirror(xy.gridX, leftRightReverse);
      var gy = xy.gridY;
      if (gx < 0 || gx > GRID_SIZE || gy < 0 || gy > GRID_SIZE) continue;
      var dot = document.createElementNS(SVG_NS, 'circle');
      dot.setAttribute('cx', gx);
      dot.setAttribute('cy', gy);
      dot.setAttribute('r', '0.3');
      dot.setAttribute('fill', '#2563EB');
      targetDotsGroup.appendChild(dot);
    }
  }

  // --- Live-connection badge (UI-SPEC Copywriting Contract) ---

  function setBadge(state) {
    badgeEl.classList.remove('is-live', 'is-connected', 'is-disconnected');
    if (state === 'live') {
      badgeEl.textContent = '● Live';
      badgeEl.classList.add('is-live');
    } else if (state === 'connected') {
      badgeEl.textContent = 'Connected';
      badgeEl.classList.add('is-connected');
    } else if (state === 'disconnected') {
      badgeEl.textContent = 'Disconnected';
      badgeEl.classList.add('is-disconnected');
    } else {
      badgeEl.textContent = 'Connecting…';
    }
  }
  setBadge('connecting');

  // --- SSE live overlay (WEBUI-03) ---
  // Named event 'target_update' — never sees the connect-time ping/state
  // dump every AsyncEventSourceResponse emits (11-RESEARCH.md Summary #2),
  // so no client-side filtering of other event names is needed.
  var sseConnected = false;
  var sse = new EventSource('/zones/events');
  sse.onopen = function () {
    // Proves the EventSource HTTP connection is open even when no
    // target_update has arrived yet (e.g. no person in view). Never
    // downgrades an already-live badge.
    if (!sseConnected) {
      setBadge('connected');
    }
  };
  sse.addEventListener('target_update', function (e) {
    if (!sseConnected) {
      sseConnected = true;
      setBadge('live');
    }
    renderTargets(decodeTargetsBase64(e.data));
  });
  sse.onerror = function () {
    // Native EventSource auto-reconnects; keep the last-drawn frame rather
    // than clearing it (UI-SPEC "Live-overlay grid" error state).
    sseConnected = false;
    setBadge('disconnected');
  };

  // --- Zone list (WEBUI-01/05) ---

  function zoneTypeOptionsHtml(selectedValue) {
    var html = '';
    for (var name in ZONE_TYPES_JS) {
      if (!Object.prototype.hasOwnProperty.call(ZONE_TYPES_JS, name)) continue;
      var value = ZONE_TYPES_JS[name];
      var selected = (selectedValue === value) ? ' selected' : '';
      html += '<option value="' + value + '"' + selected + '>' + escapeHtml(name) + '</option>';
    }
    return html;
  }

  function sensitivityOptionsHtml(selectedValue) {
    var html = '';
    [1, 2, 3].forEach(function (value) {
      var selected = (selectedValue === value) ? ' selected' : '';
      html += '<option value="' + value + '"' + selected + '>' + SENSITIVITY_LABELS[value] + '</option>';
    });
    return html;
  }

  // D-07: capacity-full state, set by loadFreeSlots(); combined with
  // savePending in updateSaveButtonsDisabled() so Add is gated by both.
  var freeSlotsFull = false;

  function updateSaveButtonsDisabled() {
    var buttons = zoneListEl.querySelectorAll('.save-btn, .remove-btn');
    for (var i = 0; i < buttons.length; i++) {
      buttons[i].disabled = savePending;
    }
    addZoneBtnEl.disabled = savePending || freeSlotsFull;
  }

  function renderZoneRow(zone) {
    var row = document.createElement('div');
    row.className = 'zone-row';
    row.setAttribute('data-zone-id', zone.id);

    var label = zone.presence_sensor ? zone.presence_sensor : ('Zone ' + zone.id);
    var zoneTypeValue = (typeof zone.zone_type === 'number') ? zone.zone_type : 0;

    row.innerHTML =
      '<div class="zone-row-header">' +
        '<span class="zone-name" title="' + escapeHtml(label) + '">' + escapeHtml(label) + '</span>' +
        '<div class="zone-controls">' +
          '<div class="field-group">' +
            '<label class="field-label" for="sensitivity-' + zone.id + '">Sensitivity</label>' +
            '<select id="sensitivity-' + zone.id + '" class="sensitivity-select">' +
              sensitivityOptionsHtml(zone.sensitivity) +
            '</select>' +
          '</div>' +
          '<div class="field-group">' +
            '<label class="field-label" for="zone-type-' + zone.id + '">Type</label>' +
            '<select id="zone-type-' + zone.id + '" class="zone-type-select">' +
              zoneTypeOptionsHtml(zoneTypeValue) +
            '</select>' +
          '</div>' +
          '<button type="button" class="save-btn">Save to Sensor</button>' +
          '<button type="button" class="remove-btn">Remove</button>' +
        '</div>' +
      '</div>' +
      '<div class="status-line"></div>';

    var saveBtn = row.querySelector('.save-btn');
    saveBtn.addEventListener('click', function () {
      handleSaveClick(zone.id, row, saveBtn);
    });

    var removeBtn = row.querySelector('.remove-btn');
    removeBtn.addEventListener('click', function () {
      handleRemoveClick(zone.id, row, removeBtn);
    });

    return row;
  }

  function renderZoneList(zones) {
    zoneListEl.innerHTML = '';
    if (!zones || zones.length === 0) {
      var empty = document.createElement('div');
      empty.className = 'empty-state';
      empty.innerHTML =
        '<h3>No zones configured</h3>' +
        '<p>This build has no compiled zones. Use the Add Zone control above to create one, or add a <code>zones:</code> block to your device YAML and reflash.</p>';
      zoneListEl.appendChild(empty);
      return;
    }
    for (var i = 0; i < zones.length; i++) {
      zoneListEl.appendChild(renderZoneRow(zones[i]));
    }
  }

  function showLoadError() {
    zoneListEl.innerHTML = '';
    var err = document.createElement('div');
    err.className = 'error-state';
    err.innerHTML =
      "<p>Couldn't load zones from the device — check it's on the same network and reachable, then reload the page.</p>" +
      '<button type="button" class="reload-btn">Reload</button>';
    zoneListEl.appendChild(err);
    var reloadBtn = err.querySelector('.reload-btn');
    reloadBtn.addEventListener('click', function () {
      window.location.reload();
    });
  }

  function loadZones() {
    zoneListEl.innerHTML = '<div class="loading-state">Loading zones…</div>';
    fetch('/api/zones')
      .then(function (resp) {
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        return resp.json();
      })
      .then(function (data) {
        mountingPosition = data.mounting_position || 'wall';
        leftRightReverse = data.left_right_reverse === true;

        // Seed editorState from this SAME response (13-RESEARCH.md Pattern
        // 1) — no new endpoint. FP2Codec.hexToGrid already handles the
        // 56-char card-format hex these fields use, and never-throws on an
        // absent/malformed grid (partial-state backstop, T-13-07).
        editorState.interference = FP2Codec.hexToGrid(data.interference_grid);
        editorState.exit = FP2Codec.hexToGrid(data.exit_grid);
        editorState.edge = FP2Codec.hexToGrid(data.edge_grid);
        // Drop zone:<id> keys for zones no longer present (loadZones() is
        // also called after a Remove/Add — WEBUI-05 finishAddRemove() below
        // — and will be reused by a future Import refresh) so a removed
        // zone's painted layer never lingers in editorState or the shared
        // zones-layer group.
        for (var staleKey in editorState) {
          if (Object.prototype.hasOwnProperty.call(editorState, staleKey) && staleKey.indexOf('zone:') === 0) {
            delete editorState[staleKey];
          }
        }
        (data.zones || []).forEach(function (z) {
          editorState['zone:' + z.id] = FP2Codec.hexToGrid(z.grid);
        });

        renderZoneList(data.zones || []);
        redrawAllLayers();
        populateLayerSelect(data.zones || []);

        // 13-UI-SPEC.md UI Considerations: Export/Import stay disabled
        // until the first successful load so neither can act on
        // stale/absent data; idempotent to call again on every later
        // successful refresh (e.g. after Add/Remove).
        exportYamlBtnEl.disabled = false;
        importDeviceBtnEl.disabled = false;
      })
      .catch(function () {
        showLoadError();
      });
  }

  // --- Add-Zone (ZONEMGMT-01, D-01/D-07): server-computed free-slot dropdown ---

  function loadFreeSlots() {
    fetch('/api/zones/free-slots')
      .then(function (resp) {
        if (!resp.ok) throw new Error('HTTP ' + resp.status);
        return resp.json();
      })
      .then(function (data) {
        var freeSlots = data.free_slots || [];
        freeSlotsFull = data.full === true || freeSlots.length === 0;

        addZoneSelectEl.innerHTML = '';
        for (var i = 0; i < freeSlots.length; i++) {
          var opt = document.createElement('option');
          opt.value = freeSlots[i];
          opt.textContent = String(freeSlots[i]);
          addZoneSelectEl.appendChild(opt);
        }
        // Discretion (D-01 follow-up): pre-select the lowest free id, which
        // is already first since json_get_free_slots() emits ids 0..31 in order.
        if (freeSlots.length > 0) addZoneSelectEl.value = String(freeSlots[0]);

        addZoneBarEl.style.display = freeSlotsFull ? 'none' : '';
        capacityMessageEl.style.display = freeSlotsFull ? '' : 'none';
        updateSaveButtonsDisabled();
      })
      .catch(function () {
        // Leave the last-known Add-Zone state as-is; loadZones()'s own
        // error path already surfaces a reload affordance for the page.
      });
  }

  function handleAddClick() {
    if (savePending || freeSlotsFull) return;
    var zoneId = addZoneSelectEl.value;
    if (zoneId === '') return;

    savePending = true;
    updateSaveButtonsDisabled();
    addZoneBtnEl.textContent = 'Adding…';

    var body = 'zone_id=' + encodeURIComponent(zoneId) + '&sensitivity=2';

    fetch('/api/zones/create', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    })
      .then(function (resp) {
        // WR-02: don't blindly poll the shared /api/zones/status endpoint on
        // a rejected POST (e.g. 409 from another in-flight save/create/delete,
        // or 400 on bad input) - that would misreport an unrelated in-flight
        // operation's outcome as this request's own result.
        if (!resp.ok) {
          return resp.json().catch(function () { return {}; }).then(function (data) {
            finishAddRemove(false, data.error || ('request rejected (' + resp.status + ')'));
          });
        }
        pollAddRemoveStatus();
      })
      .catch(function () {
        finishAddRemove(false, 'network error');
      });
  }

  function pollAddRemoveStatus() {
    fetch('/api/zones/status')
      .then(function (resp) { return resp.json(); })
      .then(function (data) {
        if (data.pending) {
          window.setTimeout(pollAddRemoveStatus, 1000);
          return;
        }
        finishAddRemove(data.ok === true, data.error);
      })
      .catch(function () {
        finishAddRemove(false, 'network error');
      });
  }

  // CR-02: surface Add/Remove failures instead of silently discarding `ok` -
  // matches finishSave()'s honest {pending, ok, error} confirmation contract
  // (WEBUI-05). Without this, a radar ACK-timeout on add/remove was
  // completely invisible: NVS + in-memory state + UI all say the change
  // succeeded (persist-first by design) even though the physical radar was
  // never actually told about it.
  function finishAddRemove(ok, errorText) {
    savePending = false;
    addZoneBtnEl.textContent = 'Add';
    updateSaveButtonsDisabled();
    if (!ok) {
      window.alert('Zone change failed: ' + (errorText || "device didn't confirm the write in time"));
    }
    loadZones();
    loadFreeSlots();
  }

  addZoneBtnEl.addEventListener('click', handleAddClick);

  // --- Remove-Zone (ZONEMGMT-02, D-06): confirm-gated destructive delete ---

  function handleRemoveClick(zoneId, row, removeBtn) {
    if (savePending) return;
    if (!window.confirm('Remove Zone ' + zoneId + '? This deletes its saved configuration.')) {
      return;
    }

    savePending = true;
    updateSaveButtonsDisabled();
    removeBtn.textContent = 'Removing…';

    var body = 'zone_id=' + encodeURIComponent(zoneId);

    fetch('/api/zones/delete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    })
      .then(function (resp) {
        // WR-02: see handleAddClick()'s identical guard.
        if (!resp.ok) {
          return resp.json().catch(function () { return {}; }).then(function (data) {
            finishAddRemove(false, data.error || ('request rejected (' + resp.status + ')'));
          });
        }
        pollAddRemoveStatus();
      })
      .catch(function () {
        finishAddRemove(false, 'network error');
      });
  }

  // --- Save flow: submit-then-poll (WEBUI-02/05, RESEARCH Pattern 3) ---

  function handleSaveClick(zoneId, row, saveBtn) {
    if (savePending) return;
    savePending = true;
    updateSaveButtonsDisabled();
    saveBtn.textContent = 'Saving…';

    var statusLine = row.querySelector('.status-line');
    statusLine.classList.remove('is-failed');
    statusLine.textContent = '';

    var sensitivitySelect = row.querySelector('.sensitivity-select');
    var zoneTypeSelect = row.querySelector('.zone-type-select');
    // Pitfall 4 (13-RESEARCH.md): grid_hex sent to /api/zones/save MUST be
    // the 80-char canonical write format (FP2Codec.gridToHex) - NEVER the
    // 56-char display format GET /api/zones returns, and NEVER a raw
    // round-trip of that GET response straight into this POST body. The
    // grid always comes from editorState (built once via FP2Codec.hexToGrid
    // at seed/import time, then mutated only by the paint/erase handlers),
    // never re-read from the wire here. A zone whose layer the user never
    // painted this session still saves correctly: editorState was already
    // seeded from the device on load, so gridToHex of that unpainted grid
    // still produces a valid 80-char string identical to what the device
    // already has.
    var zoneGrid = editorState['zone:' + zoneId];
    if (!Array.isArray(zoneGrid)) {
      // Defensive backstop only - should be unreachable, since loadZones()
      // always seeds editorState['zone:<id>'] for every rendered row before
      // its Save button can be clicked. Reuses FP2Codec.hexToGrid('')
      // (already the single source of truth for building an empty grid)
      // rather than a second empty-grid literal.
      console.warn('[FP2 Zones] handleSaveClick: no editorState grid for zone ' + zoneId + ', saving an empty grid');
      zoneGrid = FP2Codec.hexToGrid('');
    }
    var gridHex = FP2Codec.gridToHex(zoneGrid);
    var body = 'zone_id=' + encodeURIComponent(zoneId) +
      '&sensitivity=' + encodeURIComponent(sensitivitySelect.value) +
      '&zone_type=' + encodeURIComponent(zoneTypeSelect.value) +
      '&grid_hex=' + encodeURIComponent(gridHex);

    fetch('/api/zones/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    })
      .then(function (resp) {
        // WR-01 (12-REVIEW iter2): don't blindly poll the shared
        // /api/zones/status endpoint on a rejected POST (e.g. 409 from
        // another in-flight save/create/delete, or 400 on bad input) - that
        // would misreport an unrelated in-flight operation's outcome as
        // this save's own result. Matches handleAddClick()/
        // handleRemoveClick()'s WR-02 (12-REVIEW) guard exactly.
        if (!resp.ok) {
          return resp.json().catch(function () { return {}; }).then(function (data) {
            finishSave(row, saveBtn, false, data.error || ('request rejected (' + resp.status + ')'));
          });
        }
        pollSaveStatus(row, saveBtn);
      })
      .catch(function () {
        finishSave(row, saveBtn, false);
      });
  }

  function pollSaveStatus(row, saveBtn) {
    fetch('/api/zones/status')
      .then(function (resp) { return resp.json(); })
      .then(function (data) {
        if (data.pending) {
          window.setTimeout(function () { pollSaveStatus(row, saveBtn); }, 1000);
          return;
        }
        finishSave(row, saveBtn, data.ok === true, data.error);
      })
      .catch(function () {
        finishSave(row, saveBtn, false);
      });
  }

  function finishSave(row, saveBtn, ok, errorText) {
    savePending = false;
    saveBtn.textContent = 'Save to Sensor';
    updateSaveButtonsDisabled();

    var statusLine = row.querySelector('.status-line');
    if (ok) {
      statusLine.classList.remove('is-failed');
      statusLine.textContent =
        "Saved to sensor. Applied immediately — not verified by a read-back (this firmware doesn't confirm reads). Will be restored automatically from this device's saved settings on every reboot.";
    } else {
      statusLine.classList.add('is-failed');
      // WR-01 (12-REVIEW iter2): surface the specific rejection reason (e.g.
      // "a save is already in progress") when one was provided by the
      // server, mirroring finishAddRemove()'s errorText precedent, instead
      // of always showing the generic timeout message for a request that
      // was actually rejected before ever reaching the radar.
      statusLine.textContent = errorText ||
        "Save failed — the device didn't acknowledge the write in time. Nothing was confirmed applied; this zone's configuration on the device is unchanged. Check the device is online and try again.";
    }
  }

  loadZones();
  loadFreeSlots();
})();
</script>
</body>
</html>
)HTML";
static const size_t ZONES_PAGE_HTML_SIZE = sizeof(ZONES_PAGE_HTML) - 1;
