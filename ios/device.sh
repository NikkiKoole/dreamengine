#!/usr/bin/env bash
# Build, sign, and run the app on a connected physical iPhone/iPad — no Xcode GUI.
# Uses ios-deploy (classic protocol) because devicectl/CoreDevice doesn't enroll older
# devices (e.g. iOS 15) without a GUI "prepare" step; ios-deploy just works over USB.
#
#   ./device.sh                       # build signed → install → launch on the device
#   TEAM=XXXXXXXXXX ./device.sh        # override the signing team
#
# One-time prerequisites:
#   - a signing cert in the keychain: Xcode → Settings → Accounts → add your Apple ID
#     (the cert is minted on first signed build; this script triggers that).
#   - brew install ios-deploy
#   - iPhone connected + UNLOCKED (and "Trust This Computer" accepted once).
# First launch may need: iPhone → Settings → General → VPN & Device Management → trust the
# "Apple Development: <you>" profile.
set -euo pipefail
cd "$(dirname "$0")"

TEAM="${TEAM:-JH2ZCZH58D}"
SCHEME="TinyjamHello"
DEVICE_ID="${DEVICE_ID:-$(xcrun xctrace list devices 2>&1 \
  | sed -n '/== Devices ==/,/== Simulators ==/p' \
  | awk -F'[()]' '/iPhone|iPad/ {print $4; exit}')}"

[ -z "$DEVICE_ID" ] && { echo "no physical device found — connect + unlock it"; exit 1; }
echo "▸ device: $DEVICE_ID  ·  team: $TEAM"

echo "▸ generating + building (signed for device)…"
xcodegen generate --spec project.yml >/dev/null
xcodebuild -project "$SCHEME.xcodeproj" -scheme "$SCHEME" -configuration Debug \
  -destination 'generic/platform=iOS' -derivedDataPath build \
  -allowProvisioningUpdates DEVELOPMENT_TEAM="$TEAM" CODE_SIGN_STYLE=Automatic build >/dev/null

echo "▸ installing + launching on device…"
ios-deploy --id "$DEVICE_ID" --bundle "build/Build/Products/Debug-iphoneos/$SCHEME.app" --justlaunch
echo "✓ running on device"
