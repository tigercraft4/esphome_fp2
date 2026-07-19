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

  // Ported from card.js's FP2Geometry.applyGridXMirror (lines ~161-164) —
  // the single place left_right_reverse is checked for the live overlay.
  function applyGridXMirror(gridX, reverse) {
    return reverse ? (GRID_SIZE - gridX) : gridX;
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
      var gx = applyGridXMirror(xy.gridX, leftRightReverse);
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

  function updateSaveButtonsDisabled() {
    var buttons = zoneListEl.querySelectorAll('.save-btn');
    for (var i = 0; i < buttons.length; i++) {
      buttons[i].disabled = savePending;
    }
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
        '</div>' +
      '</div>' +
      '<div class="status-line"></div>';

    var saveBtn = row.querySelector('.save-btn');
    saveBtn.addEventListener('click', function () {
      handleSaveClick(zone.id, row, saveBtn);
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
        '<p>This build has no compiled zones. Add a <code>zones:</code> block to your device YAML and reflash — creating zones from this page is coming in a future update.</p>';
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
      .then(function () {
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
        finishSave(row, saveBtn, data.ok === true);
      })
      .catch(function () {
        finishSave(row, saveBtn, false);
      });
  }

  function finishSave(row, saveBtn, ok) {
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
      statusLine.textContent =
        "Save failed — the device didn't acknowledge the write in time. Nothing was confirmed applied; this zone's configuration on the device is unchanged. Check the device is online and try again.";
    }
  }

  loadZones();
})();
</script>
</body>
</html>
)HTML";
static const size_t ZONES_PAGE_HTML_SIZE = sizeof(ZONES_PAGE_HTML) - 1;
