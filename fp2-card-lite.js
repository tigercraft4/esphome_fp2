/*
 * Aqara FP2 Lite Card — minimal Home Assistant Lovelace card.
 *
 * Shows ONLY: the 14x14 detection grid, live target dots that follow movement,
 * and a global-presence badge. No zone editor / export / painting — see card.js
 * for the full-featured card. Registered as `aqara-fp2-lite-card` so both can
 * coexist.
 *
 * Config:
 *   type: custom:aqara-fp2-lite-card
 *   entity_prefix: sensor.fp2_sala        # REQUIRED (domain + device slug)
 *   targets_entity: sensor.fp2_sala_targets  # optional override
 *   mounting_position: left_upper_corner  # optional (default left_upper_corner)
 *   left_right_reverse: false             # optional (default false)
 *
 * Target decode + geometry are ported verbatim from the proven card.js
 * (decodeTargetsBase64 / targetToGridXY / X-mirror). The 14-byte target layout
 * is VERIFIED against the FP2 reverse-engineering PROTOCOL.md.
 */

const GRID = 14;
const SVG_NS = "http://www.w3.org/2000/svg";

// --- Geometry (ported verbatim from card.js FP2Geometry) ---

// Raw corner-mount coord space: X in [-400,+400], Y in [0,800]. X=+400 is the
// left edge, so the leading negation flips it into canvas/grid space where 0=left.
function targetToGridXY(rawX, rawY, mountingPosition) {
  if (!Number.isFinite(rawX) || !Number.isFinite(rawY)) return { gridX: 0, gridY: 0 };
  if (mountingPosition === "left_upper_corner" || mountingPosition === "right_upper_corner") {
    return { gridX: ((-rawX + 400) / 800.0) * GRID, gridY: (rawY / 800.0) * GRID };
  }
  // Wall mode: unverified placeholder, same as card.js.
  return { gridX: rawX * 0.01, gridY: rawY * 0.01 };
}

// left_right_reverse mirrors the grid horizontally, applied AFTER the transform
// and BEFORE the bounds check (matches card.js order).
function applyGridXMirror(gridX, leftRightReverse) {
  return leftRightReverse ? GRID - gridX : gridX;
}

// --- Target decode (ported verbatim from card.js decodeTargetsBase64) ---
// Binary: [count(1)][target(14)*count]; per-target big-endian:
// id(1), x(2), y(2), z(2), velocity(2), snr(2), classifier(1), posture(1), active(1).
function decodeTargetsBase64(base64String) {
  if (!base64String || base64String === "") return [];
  try {
    const bin = atob(base64String);
    const bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    if (bytes.length < 1) return [];
    const count = bytes[0];
    const getInt16 = (o) => {
      const v = (bytes[o] << 8) | bytes[o + 1];
      return v > 32767 ? v - 65536 : v;
    };
    const targets = [];
    for (let i = 0; i < count; i++) {
      const o = 1 + i * 14;
      if (o + 14 > bytes.length) break;
      targets.push({
        id: bytes[o],
        x: getInt16(o + 1),
        y: getInt16(o + 3),
        z: getInt16(o + 5),
        velocity: getInt16(o + 7),
        snr: getInt16(o + 9),
        classifier: bytes[o + 11],
        posture: bytes[o + 12],
        active: bytes[o + 13],
      });
    }
    return targets;
  } catch (e) {
    console.error("[FP2 Lite] base64 decode error:", e.message);
    return [];
  }
}

class AqaraFP2LiteCard extends HTMLElement {
  setConfig(config) {
    if (!config || !config.entity_prefix) {
      throw new Error("aqara-fp2-lite-card: you must define entity_prefix (e.g. sensor.fp2_sala)");
    }
    this.config = config;
    // "sensor.fp2_sala" -> device slug "fp2_sala" (for cross-domain entities).
    this._device = config.entity_prefix.replace(/^[^.]+\./, "");
    this._targetsEntity = config.targets_entity || `${config.entity_prefix}_targets`;
    this._presenceEntity = `binary_sensor.${this._device}_global_presence`;
    this._mounting = config.mounting_position || "left_upper_corner";
    this._reverse = config.left_right_reverse === true;
    this._built = false;
  }

  set hass(hass) {
    this._hass = hass;
    if (!this._built) this._build();
    this._render();
  }

  getCardSize() {
    return 4;
  }

  _build() {
    this.attachShadow({ mode: "open" });
    const style = document.createElement("style");
    style.textContent = `
      :host { display: block; }
      ha-card { padding: 12px; }
      .head { display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px; }
      .title { font-size: 15px; font-weight: 600; }
      .badge { font-size: 12px; font-weight: 600; padding: 2px 10px; border-radius: 999px; }
      .badge.on  { background: rgba(46,160,67,0.18); color: #2ea043; }
      .badge.off { background: rgba(128,128,128,0.15); color: var(--secondary-text-color); }
      .wrap { position: relative; width: 100%; aspect-ratio: 1 / 1; background: var(--card-background-color, #fff);
              border: 1px solid var(--divider-color, #e0e0e0); border-radius: 8px; overflow: hidden; }
      svg { display: block; width: 100%; height: 100%; }
      .cell { stroke: var(--divider-color, #e6e6e6); stroke-width: 0.02; fill: none; }
      .dot  { fill: #2563EB; stroke: #fff; stroke-width: 0.06; }
      .dot-label { fill: #fff; font-size: 0.5px; text-anchor: middle; dominant-baseline: central; font-weight: 700; }
      .empty { position: absolute; inset: 0; display: flex; align-items: center; justify-content: center;
               color: var(--secondary-text-color); font-size: 13px; pointer-events: none; }
    `;
    const card = document.createElement("ha-card");
    card.innerHTML = `
      <div class="head">
        <span class="title">${this.config.title || "FP2"}</span>
        <span class="badge off" id="presence">—</span>
      </div>
      <div class="wrap">
        <svg viewBox="0 0 ${GRID} ${GRID}" preserveAspectRatio="xMidYMid meet">
          <g id="grid"></g>
          <g id="dots"></g>
        </svg>
        <div class="empty" id="empty" hidden>Sem alvos</div>
      </div>
    `;
    this.shadowRoot.append(style, card);

    // Static gridlines (14x14).
    const g = card.querySelector("#grid");
    for (let i = 0; i <= GRID; i++) {
      for (const [x1, y1, x2, y2] of [[0, i, GRID, i], [i, 0, i, GRID]]) {
        const ln = document.createElementNS(SVG_NS, "line");
        ln.setAttribute("x1", x1); ln.setAttribute("y1", y1);
        ln.setAttribute("x2", x2); ln.setAttribute("y2", y2);
        ln.setAttribute("class", "cell");
        g.appendChild(ln);
      }
    }
    this._dotsEl = card.querySelector("#dots");
    this._presenceEl = card.querySelector("#presence");
    this._emptyEl = card.querySelector("#empty");
    this._built = true;
  }

  _render() {
    if (!this._hass) return;

    // Presence badge.
    const pres = this._hass.states[this._presenceEntity];
    const on = pres && pres.state === "on";
    this._presenceEl.textContent = pres ? (on ? "Presente" : "Ausente") : "—";
    this._presenceEl.className = "badge " + (on ? "on" : "off");

    // Live map config from optional overrides (kept simple — no action call).
    const mounting = this._mounting;
    const reverse = this._reverse;

    // Targets.
    const tState = this._hass.states[this._targetsEntity];
    const targets = tState ? decodeTargetsBase64(tState.state) : [];

    // Redraw dots. Draw every decoded target (do NOT filter on `active` — its
    // semantics are unreliable on real hardware; matches card.js drawTargets).
    while (this._dotsEl.firstChild) this._dotsEl.removeChild(this._dotsEl.firstChild);
    let drawn = 0;
    for (const t of targets) {
      const xy = targetToGridXY(t.x, t.y, mounting);
      const gx = applyGridXMirror(xy.gridX, reverse);
      const gy = xy.gridY;
      if (gx < 0 || gx > GRID || gy < 0 || gy > GRID) continue;
      const dot = document.createElementNS(SVG_NS, "circle");
      dot.setAttribute("cx", gx); dot.setAttribute("cy", gy);
      dot.setAttribute("r", "0.45"); dot.setAttribute("class", "dot");
      this._dotsEl.appendChild(dot);
      const label = document.createElementNS(SVG_NS, "text");
      label.setAttribute("x", gx); label.setAttribute("y", gy);
      label.setAttribute("class", "dot-label");
      label.textContent = t.id != null ? String(t.id) : "?";
      this._dotsEl.appendChild(label);
      drawn++;
    }
    this._emptyEl.hidden = drawn > 0;
  }
}

customElements.define("aqara-fp2-lite-card", AqaraFP2LiteCard);

window.customCards = window.customCards || [];
window.customCards.push({
  type: "aqara-fp2-lite-card",
  name: "Aqara FP2 Lite Card",
  description: "Minimal FP2 card: 14x14 grid + live target dots + presence badge.",
});
