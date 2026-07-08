#!/bin/bash
# Build and run the sd-bus runtime-loader test, and assert the fix property:
# a library built from sni.c + sdbus_compat.c carries NO libsystemd DT_NEEDED,
# so a single prebuilt artifact loads on systemd and systemd-free systems alike.
#
# Exit 0 only if both the link-property assertion and the functional round-trip
# pass. Requires a C toolchain and dbus-run-session; no JDK needed (the JNI
# layer is not involved).

set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Headers from any provider; never link -lsystemd (resolved at runtime).
SDBUS_CFLAGS=""
for pkg in libsystemd libelogind basu; do
    if pkg-config --exists "$pkg" 2>/dev/null; then
        SDBUS_CFLAGS=$(pkg-config --cflags "$pkg")
        echo "sd-bus headers: $pkg"
        break
    fi
done

# 1) Link-property assertion: shared object from sni.c + sdbus_compat.c must not
#    declare a libsystemd dependency, and must have no unresolved sd_bus symbols.
CHECK_SO="$SCRIPT_DIR/_libcheck.so"
gcc -shared -fPIC -O2 -Wall -Wextra -Wno-unused-parameter \
    -I "$SCRIPT_DIR" $SDBUS_CFLAGS \
    "$SCRIPT_DIR/sni.c" "$SCRIPT_DIR/sdbus_compat.c" \
    -lpthread -lm -ldl -o "$CHECK_SO" || { echo "FAIL: link check build failed"; exit 2; }

if readelf -d "$CHECK_SO" | grep -q 'NEEDED.*libsystemd'; then
    echo "FAIL: built library still declares a libsystemd DT_NEEDED"; rm -f "$CHECK_SO"; exit 1
fi
if nm -D -u "$CHECK_SO" | grep -q 'sd_bus_'; then
    echo "FAIL: unresolved sd_bus_* symbols remain (a call site is not routed)"
    nm -D -u "$CHECK_SO" | grep 'sd_bus_'; rm -f "$CHECK_SO"; exit 1
fi
rm -f "$CHECK_SO"
echo "OK: no libsystemd DT_NEEDED, no unresolved sd-bus symbols"

# 2) Functional test: resolve at runtime and complete a real bus round-trip.
BIN="$SCRIPT_DIR/test_sdbus_compat"
gcc -O2 -g -Wall -Wextra -Wno-unused-parameter \
    -I "$SCRIPT_DIR" $SDBUS_CFLAGS \
    "$SCRIPT_DIR/sdbus_compat.c" "$SCRIPT_DIR/test_sdbus_compat.c" \
    -ldl -o "$BIN" || { echo "FAIL: functional test build failed"; exit 2; }

dbus-run-session -- "$BIN"
rc=$?
rm -f "$BIN"
[ $rc -eq 0 ] && echo "PASS" || echo "FAIL: functional test exited $rc"
exit $rc
