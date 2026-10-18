#!/usr/bin/env bash
# ============================================================================
# Functional test harness for PingDD.
# Builds the Linux binary, then asserts behaviour against real local sockets.
# Requires: bash, make, python3.
# ============================================================================
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/bin/linux/pingdd"
TMP="$(mktemp -d)"
PASS=0
FAIL=0
LISTENERS=""

cleanup() {
  for pid in $LISTENERS; do kill "$pid" 2>/dev/null; done
  rm -rf "$TMP"
}
trap cleanup EXIT

ok()  { PASS=$((PASS+1)); printf '  PASS: %s\n' "$1"; }
bad() { FAIL=$((FAIL+1)); printf '  FAIL: %s\n' "$1"; }

assert_contains() { # <desc> <needle> <haystack>
  case "$3" in
    *"$2"*) ok "$1" ;;
    *)      bad "$1 (expected to contain '$2')"; printf '    got: %s\n' "$3" ;;
  esac
}
assert_rc() { # <desc> <expected> <actual>
  if [ "$2" = "$3" ]; then ok "$1"; else bad "$1 (expected rc=$2, got rc=$3)"; fi
}

# start_listener <4|6> -> echoes the bound port. Runs detached; all of its own
# fds are redirected off the script's stdout/stderr so it can never block the
# harness reader. PID is recorded for cleanup.
start_listener() {
  local fam="$1" portfile="$TMP/port.$1"
  : > "$portfile"
  python3 - "$fam" "$portfile" >/dev/null 2>&1 <<'PY' &
import socket, sys
fam = socket.AF_INET6 if sys.argv[1] == "6" else socket.AF_INET
host = "::1" if sys.argv[1] == "6" else "127.0.0.1"
s = socket.socket(fam, socket.SOCK_STREAM)
s.bind((host, 0)); s.listen(8)
with open(sys.argv[2], "w") as f:
    f.write(str(s.getsockname()[1]))
while True:
    try:
        c, _ = s.accept(); c.close()
    except OSError:
        break
PY
  LISTENERS="$LISTENERS $!"
  # Wait for the port file to be populated (max ~2s).
  for _ in $(seq 1 40); do
    [ -s "$portfile" ] && { cat "$portfile"; return 0; }
    sleep 0.05
  done
  return 1
}

echo "== building =="
make -C "$ROOT" clean  >/dev/null 2>&1
make -C "$ROOT" linux  >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 1; }
[ -x "$BIN" ] || { echo "binary missing: $BIN"; exit 1; }

echo "== argument validation =="
out="$("$BIN" -p abc -c 1 127.0.0.1 2>&1)"; rc=$?
assert_rc "non-numeric port rejected" 1 "$rc"
assert_contains "port error message" "invalid port spec" "$out"
"$BIN" -p 99999 -c 1 127.0.0.1 >/dev/null 2>&1; assert_rc "out-of-range port rejected" 1 $?
"$BIN" -c 1 127.0.0.1          >/dev/null 2>&1; assert_rc "missing port rejected" 1 $?
"$BIN" -p 443                  >/dev/null 2>&1; assert_rc "missing destination rejected" 1 $?

echo "== refused / closed port (IPv4) =="
out="$("$BIN" -p 1 -c 1 -t 1000 --no-color 127.0.0.1 2>&1)"; rc=$?
assert_contains "refused labelled correctly" "refused" "$out"
assert_rc "all-fail exits non-zero" 1 "$rc"
assert_contains "all-fail stats guarded" "no successful connections" "$out"

echo "== rate throttling =="
start_ns=$(date +%s%N)
"$BIN" -p 1 -c 3 -r 200 -t 50 --no-color 127.0.0.1 >/dev/null 2>&1
elapsed_ms=$(( ($(date +%s%N) - start_ns) / 1000000 ))
if [ "$elapsed_ms" -ge 350 ]; then
  ok "rate -r 200 honoured (${elapsed_ms}ms >= 350ms)"
else
  bad "rate ignored (${elapsed_ms}ms < 350ms)"
fi

echo "== successful connect (IPv4 listener) =="
if P4="$(start_listener 4)" && [ -n "$P4" ]; then
  out="$("$BIN" -p "$P4" -c 2 --no-color 127.0.0.1 2>&1)"; rc=$?
  assert_contains "connect success reported" "Connected to" "$out"
  assert_contains "stddev present in summary" "StdDev" "$out"
  assert_rc "success exits zero" 0 "$rc"
else
  bad "could not start IPv4 listener"
fi

echo "== successful connect (IPv6 listener) =="
if P6="$(start_listener 6)" && [ -n "$P6" ]; then
  out="$("$BIN" -p "$P6" -c 1 --no-color ::1 2>&1)"
  assert_contains "IPv6 connect success" "Connected to" "$out"
else
  echo "  SKIP: no IPv6 loopback available"
fi

echo
echo "== results: $PASS passed, $FAIL failed =="
[ "$FAIL" -eq 0 ]
