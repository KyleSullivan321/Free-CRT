#!/bin/bash
# build_mac.sh — build Free CRT for macOS (Metal GPU + CPU) into a .plugin bundle.
#
#   AE_SDK_PATH=/path/to/AfterEffectsSDK ./mac/build_mac.sh
#
# Requires: Xcode command-line tools (clang++, Rez) and the Adobe AE SDK.
#
# ⚠️  UNTESTED: this script and the Metal path were written on Windows and have
#     not been compiled/run on macOS. If something doesn't line up, the
#     authoritative reference is the AE SDK's SDK_Invert_ProcAmp Xcode project
#     (Examples/Effect/SDK_Invert_ProcAmp/Mac) — adapt flags/bundle layout from it.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${AE_SDK_PATH:?Set AE_SDK_PATH to the AE SDK root (folder containing Examples/Headers)}"
H="$AE_SDK_PATH/Examples/Headers"
U="$AE_SDK_PATH/Examples/Util"
RES="$AE_SDK_PATH/Examples/Resources"
SRC="$ROOT/src"
OUT="$ROOT/build/mac"
BUNDLE="$OUT/FreeCRT.plugin"

rm -rf "$BUNDLE"
mkdir -p "$OUT" "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"

INCLUDES="-I$H -I$H/SP -I$U -I$SRC"
ARCHS="-arch x86_64 -arch arm64"
CXXFLAGS="$ARCHS -std=c++17 -fvisibility=hidden -O2 -DMAC_ENV $INCLUDES"

echo "[1/4] Compiling (FreeCRT.cpp as Objective-C++ for Metal)..."
clang++ -c $CXXFLAGS -x objective-c++ "$SRC/FreeCRT.cpp"  -o "$OUT/FreeCRT.o"
clang++ -c $CXXFLAGS                  "$SRC/CRT_Render.cpp"  -o "$OUT/CRT_Render.o"
clang++ -c $CXXFLAGS                  "$SRC/CRT_Presets.cpp" -o "$OUT/CRT_Presets.o"
clang++ -c $CXXFLAGS                  "$SRC/CRT_Strings.cpp" -o "$OUT/CRT_Strings.o"

echo "[2/4] Linking bundle..."
clang++ $ARCHS -bundle -fvisibility=hidden \
  "$OUT/FreeCRT.o" "$OUT/CRT_Render.o" "$OUT/CRT_Presets.o" "$OUT/CRT_Strings.o" \
  -framework Metal -framework Foundation -framework CoreFoundation \
  -o "$BUNDLE/Contents/MacOS/FreeCRT"

echo "[3/4] Compiling PiPL with Rez..."
REZ="$(xcrun -find Rez)"
"$REZ" -d MAC_ENV -useDF -script Roman \
  -i "$H" -i "$H/SP" -i "$U" -i "$RES" \
  "$ROOT/resources/FreeCRT_PiPL.r" \
  -o "$BUNDLE/Contents/Resources/FreeCRT.rsrc"

echo "[4/4] Bundle metadata..."
cp "$ROOT/resources/Info.plist" "$BUNDLE/Contents/Info.plist"
printf 'eFKTFXTC' > "$BUNDLE/Contents/PkgInfo"

# ad-hoc sign so a local AE will load it (replace with your Developer ID to share)
codesign --force --deep --sign - "$BUNDLE" 2>/dev/null || echo "  (codesign skipped)"

echo "Built: $BUNDLE"
echo "Install: copy it to /Applications/Adobe After Effects <ver>/Plug-ins/"
