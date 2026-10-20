#!/bin/bash
# Builds build/spiritfm-module.zip: the release APK as a priv-app plus its privapp-permissions allowlist.
set -e
cd "$(dirname "$0")"
APK=${1:-app/build/outputs/apk/release/app-release.apk}
OUT=build/module
rm -rf "$OUT" && mkdir -p "$OUT/system/priv-app/SpiritFM"
cp -r module/. "$OUT/"
cp "$APK" "$OUT/system/priv-app/SpiritFM/SpiritFM.apk"
# System apps don't get libs extracted, so ship them beside the APK.
mkdir -p "$OUT/system/priv-app/SpiritFM/lib/arm64"
unzip -qo -j "$APK" 'lib/arm64-v8a/*' -d "$OUT/system/priv-app/SpiritFM/lib/arm64"
rm -f build/spiritfm-module.zip
(cd "$OUT" && zip -qr9 ../spiritfm-module.zip .)
echo "build/spiritfm-module.zip"
