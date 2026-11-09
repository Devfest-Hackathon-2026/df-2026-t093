#!/bin/bash
# macOS kills a Python process that touches Bluetooth unless the app that launched it declares
# NSBluetoothAlwaysUsageDescription (the crash report says "TCC ... usage description").
# This wraps `python pendant_rx.py ...` in a tiny app bundle that declares it, so macOS shows its
# normal "allow Bluetooth" prompt once for "PendantRx". Usage: tools/mac_bt_run.sh sync --out rx
set -e
APP="$HOME/.venvs/PendantRx.app"
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="${PENDANT_PY:-$HOME/.venvs/pendant/bin/python}"
LOG="${PENDANT_LOG:-/tmp/pendant_rx.log}"
mkdir -p "$APP/Contents/MacOS"
cat > "$APP/Contents/Info.plist" <<PL
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>dev.lifeos.pendantrx</string>
<key>CFBundleName</key><string>PendantRx</string>
<key>CFBundleExecutable</key><string>run</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>LSUIElement</key><true/>
<key>NSBluetoothAlwaysUsageDescription</key><string>Receive recordings from the lifeOS Pendant dev board.</string>
</dict></plist>
PL
cat > "$APP/Contents/MacOS/run" <<SH
#!/bin/bash
cd "$HERE"
exec "$PY" pendant_rx.py "\$@" > "$LOG" 2>&1
SH
chmod +x "$APP/Contents/MacOS/run"
codesign --force --sign - "$APP" >/dev/null 2>&1 || true
rm -f "$LOG"
open -W -n "$APP" --args "$@"
cat "$LOG"
