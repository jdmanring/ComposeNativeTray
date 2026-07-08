#!/bin/bash

# Build libLinuxTray.so – C-based Linux system tray library with JNI bridge.
# Dependencies: libsystemd-dev (for sd-bus), JDK (for jni.h)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="${NATIVE_LIBS_OUTPUT_DIR:-$SCRIPT_DIR/../../jvmMain/resources/composetray/native}"

echo "Building LinuxTray library (C + sd-bus + JNI)..."
echo "Output dir: $OUTPUT_DIR"

# Detect JAVA_HOME
if [ -z "$JAVA_HOME" ]; then
    # Try common locations
    for jdk in /usr/lib/jvm/java-*-openjdk-amd64 /usr/lib/jvm/java-*-openjdk /usr/lib/jvm/default-java; do
        if [ -d "$jdk" ] && [ -f "$jdk/include/jni.h" ]; then
            JAVA_HOME="$jdk"
            break
        fi
    done
fi
if [ -z "$JAVA_HOME" ] || [ ! -f "$JAVA_HOME/include/jni.h" ]; then
    echo "ERROR: JAVA_HOME not found or jni.h missing. Install a JDK or set JAVA_HOME."
    exit 1
fi
echo "Using JAVA_HOME: $JAVA_HOME"

JNI_INCLUDE="$JAVA_HOME/include"
JNI_INCLUDE_LINUX="$JAVA_HOME/include/linux"

# sd-bus HEADERS only. The library is dlopen'd at runtime (see sdbus_compat.c),
# so a single build works on systemd and systemd-free distros alike and we never
# link -lsystemd. Any provider's headers are fine: libsystemd, or elogind / basu.
SDBUS_CFLAGS=""
for pkg in libsystemd libelogind basu; do
    if pkg-config --exists "$pkg" 2>/dev/null; then
        SDBUS_CFLAGS=$(pkg-config --cflags "$pkg")
        echo "Using sd-bus headers from: $pkg"
        break
    fi
done
if [ -z "$SDBUS_CFLAGS" ] && ! echo '#include <systemd/sd-bus.h>' | gcc -E - >/dev/null 2>&1; then
    echo "ERROR: sd-bus headers not found. Install libsystemd-dev, or"
    echo "       libelogind-dev / basu on systemd-free distros."
    exit 1
fi

# Detect host architecture
UNAME_ARCH="$(uname -m)"
case "$UNAME_ARCH" in
    x86_64)  ARCH="x86-64" ;;
    aarch64) ARCH="aarch64" ;;
    *)       echo "ERROR: Unsupported architecture: $UNAME_ARCH"; exit 1 ;;
esac
PLATFORM_DIR="linux-$ARCH"
echo "Detected platform: $PLATFORM_DIR"

mkdir -p "$OUTPUT_DIR/$PLATFORM_DIR"

# Compile sni.c (includes stb_image implementation)
echo "Compiling sni.c..."
gcc -c -o "$SCRIPT_DIR/sni.o" \
    -fPIC -O2 -Wall -Wextra -Wno-unused-parameter \
    -I "$SCRIPT_DIR" \
    $SDBUS_CFLAGS \
    "$SCRIPT_DIR/sni.c"

# Compile sdbus_compat.c (runtime sd-bus loader)
echo "Compiling sdbus_compat.c..."
gcc -c -o "$SCRIPT_DIR/sdbus_compat.o" \
    -fPIC -O2 -Wall -Wextra -Wno-unused-parameter \
    -I "$SCRIPT_DIR" \
    $SDBUS_CFLAGS \
    "$SCRIPT_DIR/sdbus_compat.c"

# Compile jni_bridge.c
echo "Compiling jni_bridge.c..."
gcc -c -o "$SCRIPT_DIR/jni_bridge.o" \
    -fPIC -O2 -Wall -Wextra -Wno-unused-parameter \
    -I "$SCRIPT_DIR" \
    -I "$JNI_INCLUDE" \
    -I "$JNI_INCLUDE_LINUX" \
    "$SCRIPT_DIR/jni_bridge.c"

# Link into shared library
echo "Linking libLinuxTray.so..."
gcc -shared -o "$OUTPUT_DIR/$PLATFORM_DIR/libLinuxTray.so" \
    "$SCRIPT_DIR/sni.o" \
    "$SCRIPT_DIR/sdbus_compat.o" \
    "$SCRIPT_DIR/jni_bridge.o" \
    -lpthread -lm -ldl

# Strip debug symbols for smaller binary
strip --strip-unneeded "$OUTPUT_DIR/$PLATFORM_DIR/libLinuxTray.so"

# Clean up object files
rm -f "$SCRIPT_DIR/sni.o" "$SCRIPT_DIR/sdbus_compat.o" "$SCRIPT_DIR/jni_bridge.o"

# Invalidate runtime cache (NativeLibraryLoader validates by size only,
# so a same-size rebuild would serve the stale cached copy)
CACHE_FILE="$HOME/.cache/composetray/native/$PLATFORM_DIR/libLinuxTray.so"
if [ -f "$CACHE_FILE" ]; then
    rm -f "$CACHE_FILE"
    echo "Cleared cached library: $CACHE_FILE"
fi

echo "Build completed: $OUTPUT_DIR/$PLATFORM_DIR/libLinuxTray.so"
ls -lh "$OUTPUT_DIR/$PLATFORM_DIR/libLinuxTray.so"
