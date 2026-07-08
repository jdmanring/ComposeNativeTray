#!/bin/bash
# Build and run the issue #405 concurrency regression test.
# Runs the stress harness several times under a private D-Bus session.
# Exit 0 only if every run completes without crashing.

set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# sd-bus headers from any provider; the library is dlopen'd at runtime
# (sdbus_compat.c), so we never link -lsystemd. -ldl for the loader.
SDBUS_CFLAGS=""
for pkg in libsystemd libelogind basu; do
    if pkg-config --exists "$pkg" 2>/dev/null; then
        SDBUS_CFLAGS=$(pkg-config --cflags "$pkg")
        break
    fi
done
BIN="$SCRIPT_DIR/test_sni_concurrency"

echo "Compiling concurrency test..."
gcc -O2 -g -fPIC -Wall -Wextra -Wno-unused-parameter \
    -I "$SCRIPT_DIR" $SDBUS_CFLAGS \
    "$SCRIPT_DIR/sni.c" "$SCRIPT_DIR/sdbus_compat.c" "$SCRIPT_DIR/test_sni_concurrency.c" \
    -lpthread -lm -ldl -o "$BIN" || { echo "compile failed"; exit 2; }

RUNS="${1:-5}"
rc=0
for i in $(seq 1 "$RUNS"); do
    echo "--- run $i/$RUNS ---"
    dbus-run-session -- "$BIN"
    status=$?
    if [ $status -ne 0 ]; then
        echo "FAIL: run $i exited with status $status (signal $((status-128)) if >128)"
        rc=1
        break
    fi
done

rm -f "$BIN"
if [ $rc -eq 0 ]; then
    echo "PASS: all $RUNS runs completed without crash"
fi
exit $rc
