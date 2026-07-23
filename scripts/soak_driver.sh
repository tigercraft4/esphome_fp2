#!/usr/bin/env bash
#
# scripts/soak_driver.sh — Phase 13 (Full-Parity UI + Soak Validation),
# WEBUI-04 / Roadmap SC3.
#
# Host-side, read/SSE-only load generator for the multi-hour unattended RAM
# soak (run in Plan 13-06, on real hardware). Reproduces D-02's two-tab load
# shape and D-08's read-and-SSE-only scope against this project's REAL
# device-hosted zone editor endpoints:
#   - GET  /zones/events        (SSE, held open — zone_editor_sse_)
#   - GET  /api/zones           (zone list poll)
#   - GET  /api/zones/status    (save-status poll)
# NOT Phase 10's now-deleted /spike and built-in /events routes — those
# don't exist in this project. Phase 10's scripts/soak_driver.sh was fully
# deleted after that spike was skipped (D-10, commit 661c125); this is a
# retargeted rewrite of that precedent (shape recovered via
# `git show 7a87c84:scripts/soak_driver.sh`, further hardened per the fixes
# in commits c25f03a/73fdf8d), not a resurrection of the old file.
#
# Load shape (D-02/D-08 — READ + SSE ONLY, no write endpoints are ever
# called):
#   2x backgrounded `curl -N http://<host>/zones/events &`   (held-open SSE)
#   + a foreground loop polling GET /api/zones and
#     GET /api/zones/status every INTERVAL seconds
# This script never issues POST /api/zones/save, /api/zones/create, or
# /api/zones/delete — D-08 explicitly scopes the soak load to read/SSE only,
# no write-load simulation.
#
# A supervisor in the poll loop restarts either SSE holder if it exits
# mid-run (a held-open curl -N connection can drop over a multi-hour window
# from a transient WiFi blip or idle-socket purge, with no server-side
# fault) so the soak keeps exercising 2 concurrent SSE clients for the
# entire run instead of silently degrading to 1 or 0 with no operator
# signal — Phase 10 needed this exact fix (commit 73fdf8d).
#
# TEMPORARY: scaffolding for the Phase 13 SC3 soak only. Deleted after the
# soak concludes and the go/no-go verdict is recorded, per the D-10
# lifecycle (see 13-06's soak verdict doc for the removal step) — this is
# not permanent project tooling.
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
  echo "" >&2
  echo "This driver is READ/SSE-ONLY (D-08): it never calls" >&2
  echo "POST /api/zones/save, /api/zones/create, or /api/zones/delete." >&2
  exit 1
fi

mkdir -p "$LOG_DIR"

TIMESTAMP="$(date +%Y%m%d-%H%M%S)"
SSE_LOG_1="${LOG_DIR}/soak-sse1-${TIMESTAMP}.log"
SSE_LOG_2="${LOG_DIR}/soak-sse2-${TIMESTAMP}.log"

declare -a SSE_PIDS
SSE_PIDS=("" "")

cleanup() {
  echo "Cleaning up soak_driver.sh: stopping SSE holder(s) (pids: ${SSE_PIDS[*]})..." >&2
  for pid in "${SSE_PIDS[@]}"; do
    [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
  done
}
trap cleanup EXIT
# A signal handler registered via `trap` suppresses bash's default
# terminating behavior for that signal — the handler runs but execution
# then resumes at the point of interruption unless the handler calls `exit`
# explicitly. Without these, Ctrl-C/SIGTERM would only kill the SSE holders
# (via the EXIT trap above) while the foreground poll loop kept running
# forever (Phase 10 precedent: commit c25f03a).
trap 'exit 130' INT
trap 'exit 143' TERM

# Named so the poll loop below can restart a holder if it dies mid-soak
# (transient WiFi blip, idle-socket purge, device reboot) without silently
# degrading D-02's "2 concurrent SSE clients" condition for the rest of an
# unattended multi-hour run.
start_sse_holder() {
  local idx="$1" logfile="$2"
  curl -N "http://${HOST}/zones/events" >>"$logfile" 2>&1 &
  SSE_PIDS[$idx]="$!"
}

echo "=== soak_driver.sh (Phase 13, WEBUI-04, Roadmap SC3) ===" >&2
echo "Target host:    http://${HOST}" >&2
echo "SSE endpoint:   http://${HOST}/zones/events (2 held-open connections, D-02)" >&2
echo "Poll endpoints: http://${HOST}/api/zones and http://${HOST}/api/zones/status every ${INTERVAL}s" >&2
echo "Load scope:     READ + SSE ONLY (D-08) — no /api/zones/save|create|delete calls, ever" >&2
echo "SSE logs:       $SSE_LOG_1, $SSE_LOG_2" >&2
echo "Ctrl-C to stop cleanly (trap kills both SSE holders)." >&2
echo "==========================================================" >&2

start_sse_holder 0 "$SSE_LOG_1"
start_sse_holder 1 "$SSE_LOG_2"

echo "SSE holder PIDs: ${SSE_PIDS[*]}" >&2

# Foreground polling loop (D-02) — one short-lived GET in flight at a time
# per endpoint. Also checks and restarts either SSE holder if it has died,
# logging the restart so an operator reviewing the log can see it happened.
while true; do
  for idx in 0 1; do
    if ! kill -0 "${SSE_PIDS[$idx]}" 2>/dev/null; then
      logfile=$([ "$idx" = 0 ] && echo "$SSE_LOG_1" || echo "$SSE_LOG_2")
      echo "$(date -u +%FT%TZ) SSE holder $idx died, restarting" >&2
      start_sse_holder "$idx" "$logfile"
    fi
  done
  curl -s -o /dev/null -w "%{time_total}s /api/zones -> HTTP %{http_code}\n" "http://${HOST}/api/zones" || true
  curl -s -o /dev/null -w "%{time_total}s /api/zones/status -> HTTP %{http_code}\n" "http://${HOST}/api/zones/status" || true
  sleep "$INTERVAL"
done
