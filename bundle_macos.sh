#!/bin/bash
# Tool to bundle dynamic libraries into the application for macOS portability.
# This script creates a self-contained .app bundle.
# Requires dylibbundler: brew install dylibbundler

TARGET="${TARGET:-P2ProViewer}"
BUILD_DIR="${BUILD_DIR:-cmake-build-debug}"

if [ -d "$BUILD_DIR/$TARGET.app" ]; then
    echo "Using existing app bundle from build directory..."
    # If it's a bundle, the executable we want to fix is inside it
    # But wait, the script currently creates its OWN bundle structure.
    # If CMake already created one, we should probably just use it or 
    # fix the one it created.
    
    # Let's stick to the current script's logic of creating a NEW bundle structure
    # in the current directory, but make sure we find the executable.
    SOURCE_EXE="$BUILD_DIR/$TARGET.app/Contents/MacOS/$TARGET"
    if [ ! -f "$SOURCE_EXE" ]; then
        # Fallback to just the executable if it's not in the bundle
        SOURCE_EXE="$BUILD_DIR/$TARGET"
    fi
else
    SOURCE_EXE="$BUILD_DIR/$TARGET"
fi

if [ ! -f "$SOURCE_EXE" ]; then
    echo "Executable $SOURCE_EXE not found. Please build the project first."
    exit 1
fi

echo "Creating bundle structure..."
mkdir -p "$TARGET.app/Contents/MacOS"
mkdir -p "$TARGET.app/Contents/Resources"
cp "$SOURCE_EXE" "$TARGET.app/Contents/MacOS/$TARGET"

# Copy icon if it exists
if [ -f "Resources/P2ProViewer.icns" ]; then
    cp "Resources/P2ProViewer.icns" "$TARGET.app/Contents/Resources/"
fi

# Create a basic PkgInfo
echo "APPL????" > "$TARGET.app/Contents/PkgInfo"

# Create a basic Info.plist if it doesn't exist
if [ ! -f "$TARGET.app/Contents/Info.plist" ]; then
cat <<EOF > "$TARGET.app/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>$TARGET</string>
    <key>CFBundleGetInfoString</key>
    <string>$TARGET Viewer</string>
    <key>CFBundleIdentifier</key>
    <string>com.p2pro.$TARGET</string>
    <key>CFBundleName</key>
    <string>$TARGET</string>
    <key>CFBundleIconFile</key>
    <string>P2ProViewer.icns</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
</dict>
</plist>
EOF
fi

if command -v dylibbundler &> /dev/null
then
    echo "Running dylibbundler..."
    # -od: overwrite directory
    # -b: bundle libs
    # -x: executable
    # -d: destination for libs
    # -p: fix inner paths
    dylibbundler -od -b -x "$TARGET.app/Contents/MacOS/$TARGET" -d "$TARGET.app/Contents/Libs/" -p "@executable_path/../Libs/"
    echo "Bundling complete. Created $TARGET.app"
else
    echo "--------------------------------------------------------------------------------"
    echo "Warning: dylibbundler not found!"
    echo "To make the application truly portable (statically bundle dependencies),"
    echo "please install dylibbundler using Homebrew:"
    echo ""
    echo "    brew install dylibbundler"
    echo ""
    echo "Then run this script again."
    echo "--------------------------------------------------------------------------------"
fi
