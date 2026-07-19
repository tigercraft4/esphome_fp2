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
/* Task 2 populates this block: fetch/render, SSE overlay, save flow. */
</script>
</body>
</html>
)HTML";
static const size_t ZONES_PAGE_HTML_SIZE = sizeof(ZONES_PAGE_HTML) - 1;
