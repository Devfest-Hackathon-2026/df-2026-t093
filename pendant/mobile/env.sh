# Sourced by verify.sh and by the Xcode "Compile Kotlin" build phase. Exports only into the calling process:
# nothing under ~/.gradle, no shell rc, no global git config is touched.
# JAVA_HOME: the environment's, else Homebrew openjdk@17, else @21. ANDROID_HOME / ANDROID_SDK_ROOT: Homebrew's command-line tools.
if [ -z "${JAVA_HOME:-}" ]; then
  for j in /opt/homebrew/opt/openjdk@17 /opt/homebrew/opt/openjdk@21; do
    if [ -d "$j/libexec/openjdk.jdk/Contents/Home" ]; then JAVA_HOME="$j/libexec/openjdk.jdk/Contents/Home"; break; fi
    if [ -d "$j" ] && [ -x "$j/bin/java" ]; then JAVA_HOME="$j"; break; fi
  done
fi
export JAVA_HOME
export ANDROID_HOME="${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}"
export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$ANDROID_HOME}"
