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
  #live-grid-container {
    border: 1px solid #D8DCE1;
    border-radius: 4px;
    background: #F1F3F5;
    line-height: 0;
  }
  #live-grid { display: block; width: 100%; height: auto; }
  #zone-list { max-height: 480px; overflow-y: auto; }
  .zone-row {
    border-bottom: 1px solid #D8DCE1;
    padding: 16px 0;
  }
  .zone-row:last-child { border-bottom: none; }
  .zone-row-header {
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

  <section class="panel" id="live-view-panel">
    <h2 class="panel-title">Live View</h2>
    <div id="live-grid-container">
      <svg id="live-grid" viewBox="0 0 14 14" preserveAspectRatio="xMidYMid meet"></svg>
    </div>
  </section>

  <section class="panel" id="zones-panel">
    <h2 class="panel-title">Zones</h2>
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
  var GRID_SIZE = 14;
  var SVG_NS = 'http://www.w3.org/2000/svg';

  var badgeEl = document.getElementById('live-badge');
  var zoneListEl = document.getElementById('zone-list');
  var liveGridEl = document.getElementById('live-grid');
  var addZoneBarEl = document.getElementById('add-zone-bar');
  var addZoneSelectEl = document.getElementById('add-zone-select');
  var addZoneBtnEl = document.getElementById('add-zone-btn');
  var capacityMessageEl = document.getElementById('capacity-message');

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
    badgeEl.classList.remove('is-live', 'is-disconnected');
    if (state === 'live') {
      badgeEl.textContent = '● Live';
      badgeEl.classList.add('is-live');
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
        renderZoneList(data.zones || []);
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
    var body = 'zone_id=' + encodeURIComponent(zoneId) +
      '&sensitivity=' + encodeURIComponent(sensitivitySelect.value) +
      '&zone_type=' + encodeURIComponent(zoneTypeSelect.value);

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
