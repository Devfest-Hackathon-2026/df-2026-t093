#!/usr/bin/env bash
# One entry point for everything the mobile app can be checked with, in this order:
#   1 shared tests (host JVM / Android unit test)   2 C interop tests   3 Android assembleDebug + manifest check
#   4 iOS simulator tests and iosApp build (macOS with Xcode only)
# Stops with a non-zero exit at the first failure. Ends with a PASS / FAIL / SKIPPED table.
# MOBILE_REQUIRE_IOS=1 turns an iOS SKIPPED into a failure (CI on macOS uses it).
set -euo pipefail
cd "$(dirname "$0")"
. ./env.sh # JAVA_HOME / ANDROID_HOME for this process only

names=(); results=(); notes=()
row() { names+=("$1"); results+=("$2"); notes+=("${3:-}"); }
table() {
  echo; echo "mobile/verify.sh (JAVA_HOME=$JAVA_HOME)"
  printf '%-44s %-8s %s\n' STEP RESULT NOTE
  for i in "${!names[@]}"; do printf '%-44s %-8s %s\n' "${names[$i]}" "${results[$i]}" "${notes[$i]}"; done
}
# step <name> <command...>: PASS, or FAIL + table + exit 1
step() {
  local name="$1"; shift
  echo "== $name"
  if "$@"; then row "$name" PASS; else row "$name" FAIL "see output above"; table; exit 1; fi
}
gradle() { ./gradlew --console=plain "$@"; }

step "1 shared tests (host JVM)" gradle cleanTestDebugUnitTest :shared:testDebugUnitTest

if command -v cc >/dev/null 2>&1; then
  step "2 C interop tests (fw/lib sender)" gradle cleanTestDebugUnitTest :shared:testDebugUnitTest -PinteropOnly
else
  row "2 C interop tests (fw/lib sender)" SKIPPED "no C compiler (cc) found"
fi

step "3a Android assembleDebug" gradle :androidApp:assembleDebug
no_internet() {
  local m; m=$(ls androidApp/build/intermediates/merged_manifests/debug/*/AndroidManifest.xml 2>/dev/null | head -1)
  [ -n "$m" ] || { echo "merged manifest not found"; return 1; }
  if grep -q 'android.permission.INTERNET' "$m"; then echo "INTERNET is in $m"; return 1; fi
}
step "3b merged manifest has no INTERNET" no_internet

ios_skip=""
if [ "$(uname -s)" != Darwin ]; then ios_skip="not macOS"
elif ! command -v xcodebuild >/dev/null 2>&1 || ! xcodebuild -version >/dev/null 2>&1; then ios_skip="xcodebuild not found"
elif ! xcrun simctl list runtimes available 2>/dev/null | grep -q 'iOS '; then ios_skip="no iOS Simulator runtime"
fi
if [ -n "$ios_skip" ]; then
  row "4a shared iosSimulatorArm64 tests" SKIPPED "$ios_skip"
  row "4b iosApp build (generic iOS Simulator)" SKIPPED "$ios_skip"
else
  step "4a shared iosSimulatorArm64 tests" gradle :shared:iosSimulatorArm64Test
  ios_build() {
    xcodebuild -project iosApp/iosApp.xcodeproj -scheme iosApp -destination 'generic/platform=iOS Simulator' \
      -configuration Debug -derivedDataPath iosApp/build CODE_SIGNING_ALLOWED=NO build
  }
  step "4b iosApp build (generic iOS Simulator)" ios_build
fi

table
if [ -n "$ios_skip" ] && [ "${MOBILE_REQUIRE_IOS:-0}" = 1 ]; then echo "MOBILE_REQUIRE_IOS=1 but iOS was skipped ($ios_skip)"; exit 1; fi
echo "OK"
