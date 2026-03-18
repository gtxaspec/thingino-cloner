#!/bin/sh
# Download official libusb Windows release for cross-compilation.
# Caches in third_party/libusb/windows/ to avoid re-downloading.
set -e

VERSION="${LIBUSB_VERSION:-1.0.27}"
CACHE_DIR="third_party/libusb/windows"
STAMP="$CACHE_DIR/.version"

# Check if already downloaded
if [ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$VERSION" ]; then
    exit 0
fi

URL="https://github.com/libusb/libusb/releases/download/v${VERSION}/libusb-${VERSION}.7z"
TMPFILE=$(mktemp)
TMPDIR=$(mktemp -d)

echo "Downloading libusb ${VERSION} for Windows..."
curl -fSL "$URL" -o "$TMPFILE"

if command -v 7z >/dev/null 2>&1; then
    7z x -o"$TMPDIR" "$TMPFILE" >/dev/null
elif command -v 7zz >/dev/null 2>&1; then
    7zz x -o"$TMPDIR" "$TMPFILE" >/dev/null
else
    echo "Error: 7z required. Install: apt install p7zip-full"
    rm -f "$TMPFILE"
    exit 1
fi
rm -f "$TMPFILE"

# Archive layout (libusb 1.0.27):
#   include/libusb.h
#   MinGW64/dll/libusb-1.0.dll, MinGW64/static/libusb-1.0.a + .dll.a
#   MinGW32/dll/..., MinGW32/static/...
rm -rf "$CACHE_DIR"
mkdir -p "$CACHE_DIR"

for arch in x86_64 x86; do
    case $arch in
        x86_64) mingw="MinGW64" ;;
        x86)    mingw="MinGW32" ;;
    esac

    dst="$CACHE_DIR/$arch"
    mkdir -p "$dst/include/libusb-1.0" "$dst/lib" "$dst/bin"

    # Header
    cp "$TMPDIR/include/libusb.h" "$dst/include/libusb-1.0/"

    # DLL
    cp "$TMPDIR/$mingw/dll/libusb-1.0.dll" "$dst/bin/"

    # Import libs (for linking)
    cp "$TMPDIR/$mingw/static/libusb-1.0.a" "$dst/lib/" 2>/dev/null || true
    cp "$TMPDIR/$mingw/static/libusb-1.0.dll.a" "$dst/lib/" 2>/dev/null || true
done

rm -rf "$TMPDIR"
echo "$VERSION" > "$STAMP"
echo "libusb ${VERSION} ready in $CACHE_DIR/"
