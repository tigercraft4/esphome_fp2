#!/usr/bin/env bash
#
# scripts/soak_summary.sh — Phase 13 (Full-Parity UI + Soak Validation),
# WEBUI-04 / Roadmap SC3.
#
# Log-correlation aggregator for the go/no-go verdict (Plan 13-06). Reads
# soak_driver.sh's SSE/poll log(s) plus a captured device log (containing
# the "soak_heap" RAM dump from fp2-sala.yaml's temporary heap-caps sensors
# and/or the radar_debug text sensor's UART frame diagnostics) and produces:
#   (1) a HARD-FAILURE verdict (D-02's ONLY gating signal) — watchdog reset,
#       crash/reboot, or dropped/garbled UART frames — non-zero exit if any
#       are found, with an explicit "HARD FAILURE DETECTED" line.
#   (2) a SOFT-SIGNAL report (declining free heap / rising fragmentation) —
#       informational only, printed but never affecting the exit code
#       (D-02: soft signals are logged/reported, never fail the test alone).
#
# Rebuilt from scratch per D-10 (Phase 10's scripts/soak_summary.sh was
# fully deleted after that spike was skipped — commit 661c125). Retargeted
# from the deleted spike_soak/`/spike` log vocabulary (recovered via
# `git show 7036fb9:scripts/soak_summary.sh`) to this project's real
# soak_heap tag and radar_debug entity, and extended with the explicit
# hard-failure-exit-code + soft-signal-only-informational behavior this
# phase's plan requires (Phase 10's version reported both series but never
# actually gated the exit code on the hard-failure scan).
#
# Usage:
#   scripts/soak_summary.sh <log-file> [<log-file> ...]
#
# Pass every log captured during the run: soak_driver.sh's own
# stdout/stderr capture (SSE holder restarts, poll HTTP codes) and/or a
# captured device log (e.g. `esphome logs fp2-sala.yaml > device.log`)
# containing the soak_heap dump and radar_debug output. All files are
# concatenated before scanning, so argument order does not matter.
#
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <log-file> [<log-file> ...]" >&2
  exit 1
fi

for f in "$@"; do
  if [ ! -r "$f" ]; then
    echo "Error: log file '$f' does not exist or is not readable." >&2
    exit 1
  fi
done

ALL_LOGS="$(cat -- "$@")"

echo "=== soak_heap RAM series (free=/min_free=/frag=, fp2-sala.yaml temporary sensors) ==="
HEAP_LINES="$(printf '%s\n' "$ALL_LOGS" | grep -F 'soak_heap' || true)"

if [ -z "$HEAP_LINES" ]; then
  echo "No soak_heap lines found in the provided log(s)."
else
  echo "$HEAP_LINES"
  echo
  echo "--- soak_heap summary (soft signals — informational only, D-02) ---"
  # LC_ALL=C is required here: awk's string-to-number conversion is
  # locale-sensitive, and under a comma-decimal locale (e.g. pt_PT)
  # "5.5"+0 silently truncates to 5 instead of 5.5, corrupting every
  # min/max/last comparison (Phase 10 finding, still applicable).
  echo "$HEAP_LINES" | LC_ALL=C awk '
    {
      free = ""; min_free = ""; frag = "";
      for (i = 1; i <= NF; i++) {
        n_parts = split($i, parts, "=");
        if (n_parts != 2) continue;
        key = parts[1]; val = parts[2];
        gsub(/[^0-9.]/, "", val);
        if (val == "") continue;
        if (key == "free") free = val;
        else if (key == "min_free") min_free = val;
        else if (key == "frag") frag = val;
      }
      # Seed min/max per-field on first sight of THAT field, not on a
      # shared line counter — keying off a shared counter left a field
      # stuck at "" (coerced to 0 by awk) for the whole run if the first
      # matched line happened to be missing that field (Phase 10 fix,
      # commit 7036fb9).
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
      if (frag != "") {
        if (frag_min == "") { frag_min = frag; frag_max = frag; }
        if (frag + 0 < frag_min + 0) frag_min = frag;
        if (frag + 0 > frag_max + 0) frag_max = frag;
        frag_last = frag;
      }
      n++;
    }
    END {
      if (n == 0) { print "  (no parseable soak_heap fields found)"; exit; }
      printf "  samples:       %d\n", n;
      printf "  free:          min=%s max=%s last=%s\n", free_min, free_max, free_last;
      printf "  min_free:      min=%s max=%s last=%s\n", minfree_min, minfree_max, minfree_last;
      printf "  fragmentation: min=%s max=%s last=%s (rising trend if last > min)\n", frag_min, frag_max, frag_last;
    }
  '
fi

echo
echo "=== Hard-failure scan (D-02's ONLY gating signal: watchdog reset, crash/reboot, dropped/garbled UART frames) ==="
# Broad, case-insensitive patterns covering: ESP-IDF task-watchdog trigger,
# a hardware/software reset caused by the RTC or task watchdog, a crash
# (Guru Meditation / abort / backtrace / boot-time reset marker), and the
# radar_debug text sensor's own UART frame-loss/CRC-fail vocabulary.
HARD_FAILURE_LINES="$(printf '%s\n' "$ALL_LOGS" | grep -iE \
  'task watchdog|rtcwdt|tg0wdt|tg1wdt|guru meditation|abort\(\)|backtrace:|rst:0x|CRC Fail|dropped.*frame|garbled|frame.loss' \
  || true)"

HARD_FAILURE=0
if [ -n "$HARD_FAILURE_LINES" ]; then
  HARD_FAILURE=1
  echo "$HARD_FAILURE_LINES"
else
  echo "No hard-failure indicators found — absence is the pass signal per D-02/D-03."
fi

echo
echo "=== radar_debug entries (manual cross-reference against the soak_heap timeline) ==="
RADAR_DEBUG_LINES="$(printf '%s\n' "$ALL_LOGS" | grep -iE 'radar_debug|radar debug' || true)"
if [ -z "$RADAR_DEBUG_LINES" ]; then
  echo "No radar_debug entries found in the provided log(s)."
else
  echo "$RADAR_DEBUG_LINES"
fi

echo
echo "=== VERDICT ==="
if [ "$HARD_FAILURE" -eq 1 ]; then
  echo "HARD FAILURE DETECTED — see the hard-failure scan above (D-02/D-03: watchdog reset, crash, or dropped/garbled UART frames)."
  echo "FAIL"
  exit 1
else
  echo "No hard failures detected. Soft signals (heap decline, rising fragmentation, if any) are reported above for the record but do NOT affect this verdict (D-02)."
  echo "PASS"
  exit 0
fi
