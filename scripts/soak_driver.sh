#!/usr/bin/env bash
#
# scripts/soak_driver.sh — Phase 10 (Architecture Validation Spike), SPIKE-01.
#
# Host-side synthetic load generator for the 4-6h unattended soak (plan 10-03).
# Reproduces decisions D-01 (unattended, hours-long run), D-02 (periodic /spike
# polling), and D-07 (2 concurrent held-open SSE clients) from
# 10-RESEARCH.md's Open Question 3 recommendation:
#   2x backgrounded `curl -N http://<ip>/events &` (real, held-open SSE — D-05)
#   + a foreground `while true; do curl http://<ip>/spike; sleep <n>; done` loop
#   + trap-based cleanup on exit/interrupt.
#
# Socket budget note (Pitfall 5): this script deliberately keeps its footprint
# at exactly 2 held-open SSE connections + one short-lived GET in flight at a
# time, well within ESP-IDF's httpd max_open_sockets=7 ceiling. Do not add more
# concurrent connections here without re-checking that budget.
#
# TEMPORARY: this script is scaffolding for the Phase 10 spike only and is
# deleted after the soak concludes (plan 10-03), regardless of go/no-go verdict.
#
# Usage:
#   scripts/soak_driver.sh <device-ip-or-host> [poll-interval-seconds] [log-dir]
#   DEVICE_IP=<device-ip-or-host> scripts/soak_driver.sh
#
set -euo pipefail

HOST="${1:-${DEVICE_IP:-}}"
INTERVAL="${2:-2}"
LOG_DIR="${3:-.}"

if [ -z "$HOST" ]; then
  echo "Usage: $0 <device-ip-or-host> [poll-interval-seconds] [log-dir]" >&2
  echo "       (or set DEVICE_IP env var instead of the first argument)" >&2
  exit 1
fi

mkdir -p "$LOG_DIR"

TIMESTAMP="$(date +%Y%m%d-%H%M%S)"
SSE_LOG_1="${LOG_DIR}/soak-sse1-${TIMESTAMP}.log"
SSE_LOG_2="${LOG_DIR}/soak-sse2-${TIMESTAMP}.log"

SSE_PIDS=()

cleanup() {
  echo "Cleaning up soak_driver.sh: killing ${#SSE_PIDS[@]} SSE holder(s)..." >&2
  for pid in "${SSE_PIDS[@]}"; do
    kill "$pid" 2>/dev/null || true
  done
}
trap cleanup EXIT
# CR-01: a signal handler registered via `trap` suppresses bash's default
# terminating behavior for that signal - the handler runs but execution then
# resumes at the point of interruption unless the handler calls `exit`
# explicitly. Without these, Ctrl-C/SIGTERM only killed the SSE holders while
# the foreground poll loop below kept running forever.
trap 'exit 130' INT
trap 'exit 143' TERM

# CR-02: named so the poll loop below can restart a holder if it dies
# mid-soak (transient WiFi blip, idle-socket purge, device reboot) without
# silently degrading D-07's "2 concurrent SSE clients" condition for the
# rest of an unattended multi-hour run.
start_sse_holder() {
  local idx="$1" logfile="$2"
  curl -N "http://${HOST}/events" >"$logfile" 2>&1 &
  SSE_PIDS[$idx]="$!"
}

echo "Starting 2 persistent SSE holders against http://${HOST}/events (D-05/D-07)..." >&2
start_sse_holder 0 "$SSE_LOG_1"
start_sse_holder 1 "$SSE_LOG_2"

echo "SSE holder PIDs: ${SSE_PIDS[*]} (logs: $SSE_LOG_1, $SSE_LOG_2)" >&2
echo "Polling http://${HOST}/spike every ${INTERVAL}s (D-02). Ctrl-C to stop cleanly." >&2

# Foreground polling loop (D-02) — one short-lived GET in flight at a time,
# keeping total concurrent sockets at 2 SSE + 1 poll (Pitfall 5). Also
# checks and restarts either SSE holder if it has died (CR-02), logging the
# restart so an operator reviewing the log can see it happened.
while true; do
  for idx in 0 1; do
    if ! kill -0 "${SSE_PIDS[$idx]}" 2>/dev/null; then
      logfile=$([ "$idx" = 0 ] && echo "$SSE_LOG_1" || echo "$SSE_LOG_2")
      echo "$(date -u +%FT%TZ) SSE holder $idx died, restarting" >&2
      start_sse_holder "$idx" "$logfile"
    fi
  done
  curl -s -o /dev/null -w "%{time_total}s spike poll -> HTTP %{http_code}\n" "http://${HOST}/spike" || true
  sleep "$INTERVAL"
done
