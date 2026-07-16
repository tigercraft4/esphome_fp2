#!/usr/bin/env bash
#
# scripts/soak_summary.sh — Phase 10 (Architecture Validation Spike), SPIKE-01.
#
# Log-correlation helper for the go/no-go verdict (plan 10-03). Turns a raw
# captured soak log into a single comparable timeline per D-04/D-11:
#   - the periodic "spike_soak" ESP_LOGI dump (free=/block=/min_free=/frag=/
#     loop_time=, Pattern 4 in 10-RESEARCH.md) -> baseline-vs-loaded RAM/timing
#     summary (min/max/last free, min_free, loop_time; max fragmentation).
#   - the radar_debug / UART frame-loss / CRC-fail entries (D-03's hard-failure
#     signal), printed interleaved by timestamp with the spike_soak series so
#     a loop_time/heap anomaly can be time-correlated against actual UART
#     frame loss before calling a hard failure (Pitfall 4).
#
# TEMPORARY: scaffolding for the Phase 10 spike only; deleted after the soak
# concludes (plan 10-03), regardless of go/no-go verdict.
#
# Usage:
#   scripts/soak_summary.sh <captured-soak-log-file>
#
set -euo pipefail

LOG_FILE="${1:-}"

if [ -z "$LOG_FILE" ]; then
  echo "Usage: $0 <captured-soak-log-file>" >&2
  exit 1
fi

if [ ! -r "$LOG_FILE" ]; then
  echo "Error: log file '$LOG_FILE' does not exist or is not readable." >&2
  exit 1
fi

echo "=== spike_soak RAM/timing series (Pattern 4: free=/block=/min_free=/frag=/loop_time=) ==="
SPIKE_LINES="$(grep -F 'spike_soak' "$LOG_FILE" || true)"

if [ -z "$SPIKE_LINES" ]; then
  echo "No spike_soak lines found in $LOG_FILE."
else
  echo "$SPIKE_LINES"
  echo
  echo "--- spike_soak summary (baseline vs loaded, D-11) ---"
  # LC_ALL=C is required here: awk's string-to-number conversion is locale-
  # sensitive, and under a comma-decimal locale (e.g. pt_PT) "5.5"+0 silently
  # truncates to 5 instead of 5.5, corrupting every min/max/last comparison.
  echo "$SPIKE_LINES" | LC_ALL=C awk '
    # Uses split()-on-"=" instead of 3-arg match() for portability with
    # POSIX/BSD awk (macOS default awk lacks gawk-style match() capture groups).
    {
      free = ""; min_free = ""; frag = ""; loop_time = "";
      for (i = 1; i <= NF; i++) {
        n_parts = split($i, parts, "=");
        if (n_parts != 2) continue;
        key = parts[1]; val = parts[2];
        gsub(/[^0-9.]/, "", val);
        if (val == "") continue;
        if (key == "free") free = val;
        else if (key == "min_free") min_free = val;
        else if (key == "frag") frag = val;
        else if (key == "loop_time") loop_time = val;
      }
      # WR-03: seed min/max per-field on first sight of THAT field, not on
      # the shared line counter n==0. Keying off n==0 left a field stuck at
      # "" (coerced to 0 by awk) for the whole run if the first matched
      # line happened to be missing that field (e.g. a truncated capture).
      if (free != "") {
        if (free_min == "") { free_min = free; free_max = free; }
        if (free + 0 < free_min + 0) free_min = free;
        if (free + 0 > free_max + 0) free_max = free;
        free_last = free;
      }
      if (min_free != "") {
        if (minfree_min == "") { minfree_min = min_free; minfree_max = min_free; }
        if (min_free + 0 < minfree_min + 0) minfree_min = min_free;
        if (min_free + 0 > minfree_max + 0) minfree_max = min_free;
        minfree_last = min_free;
      }
      if (loop_time != "") {
        if (loop_min == "") { loop_min = loop_time; loop_max = loop_time; }
        if (loop_time + 0 < loop_min + 0) loop_min = loop_time;
        if (loop_time + 0 > loop_max + 0) loop_max = loop_time;
        loop_last = loop_time;
      }
      if (frag != "" && (frag + 0 > frag_max + 0 || n == 0)) frag_max = frag;
      n++;
    }
    END {
      if (n == 0) { print "  (no parseable spike_soak fields found)"; exit; }
      printf "  samples:      %d\n", n;
      printf "  free:         min=%s max=%s last=%s\n", free_min, free_max, free_last;
      printf "  min_free:     min=%s max=%s last=%s\n", minfree_min, minfree_max, minfree_last;
      printf "  loop_time:    min=%s max=%s last=%s\n", loop_min, loop_max, loop_last;
      printf "  fragmentation: max=%s\n", frag_max;
    }
  '
fi

echo
echo "=== radar_debug / UART frame-loss / CRC-fail entries (D-03 hard-failure signal) ==="
# Match the published "radar_debug" text-sensor entity as well as the raw
# CRC/frame-loss log lines it is fed from (Pitfall 4) so this is robust to
# whichever capture format the operator's log tool produces.
DEBUG_LINES="$(grep -iE 'radar_debug|radar debug|CRC Fail|dropped.*frame|garbled|frame.loss' "$LOG_FILE" || true)"

if [ -z "$DEBUG_LINES" ]; then
  echo "No hard-failure UART lines found (no radar_debug frame-loss/CRC-fail entries) — absence is the pass signal per D-03."
else
  echo "$DEBUG_LINES"
fi

echo
echo "=== Correlated timeline (spike_soak + radar_debug, sorted by log order) ==="
if [ -z "$SPIKE_LINES" ] && [ -z "$DEBUG_LINES" ]; then
  echo "(nothing to correlate — neither series had matching lines)"
else
  grep -inE 'spike_soak|radar_debug|radar debug|CRC Fail|dropped.*frame|garbled|frame.loss' "$LOG_FILE" || true
fi
