#!/usr/bin/env bash
# Repeatable M0 benchmark run: host benches + Android stats via adb.
# Works against an emulator or a physical device, whichever adb sees first.
# Results are labelled accordingly - the script never upgrades a label.
# Usage: scripts/run_benchmarks.sh [output.md]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

ADB="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
OUT="${1:-docs/benchmark-results/$(date +%F)-run.md}"
mkdir -p "$(dirname "$OUT")"
HOST_BIN=/tmp/opencode/host-tests

{
  echo "# Benchmark run $(date -Iseconds)"
  echo
} > "$OUT"

echo "== Host build & tests =="
cmake -S cpp/tests -B "$HOST_BIN" -DCMAKE_BUILD_TYPE=Release >>"$OUT" 2>&1
cmake --build "$HOST_BIN" -j4 >>"$OUT" 2>&1
ctest --test-dir "$HOST_BIN" 2>&1 | tee -a "$OUT" | tail -2

{
  echo
  echo "## Host import (median of 3)"
  echo "| Fixture | Median ms |"
  echo "|---|---|"
} >> "$OUT"
for f in cpp/tests/fixtures/sphere_*.glb; do
  median=$("$HOST_BIN/bench_import" "$f" 3 | awk '/^median:/{print $2}')
  echo "| $(basename "$f") | $median |" >> "$OUT"
done

{
  echo
  echo "## Host deform (60 iterations, ~4k vertex region)"
  echo '```'
} >> "$OUT"
for f in sphere_98k sphere_393k; do
  echo "-- $f" >> "$OUT"
  "$HOST_BIN/bench_deform" "cpp/tests/fixtures/$f.glb" 60 >> "$OUT"
done
echo '```' >> "$OUT"

echo "== Android build =="
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/default}"
export ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"
S=$(date +%s)
./gradlew :app:assembleDebug --console=plain 2>&1 | grep -E "BUILD" || true
echo "assembleDebug: $(( $(date +%s) - S ))s" | tee -a "$OUT"

DEVICE_COUNT=$("$ADB" devices | awk 'NR>1 && $2=="device"' | wc -l)
if [ "$DEVICE_COUNT" -eq 0 ]; then
  echo "No adb device/emulator available - Android section skipped." | tee -a "$OUT"
  echo "Wrote $OUT"
  exit 0
fi

MODEL=$("$ADB" shell getprop ro.product.model | tr -d '\r')
SDK=$("$ADB" shell getprop ro.build.version.sdk | tr -d '\r')
EMU=$("$ADB" shell getprop ro.boot.qemu | tr -d '\r')
if [ "$EMU" = "1" ]; then LABEL="Emulator Tested"; else LABEL="Verified on Physical Device"; fi

{
  echo
  echo "## Android ($LABEL)"
  echo "- Model: $MODEL, SDK: $SDK"
  echo "- 3 cold runs; load ms from logcat, PSS from dumpsys meminfo"
  echo
  echo "| Run | Load ms | PSS KB |"
  echo "|---|---|---|"
} >> "$OUT"

"$ADB" install -r app/build/outputs/apk/debug/app-debug.apk >/dev/null
for run in 1 2 3; do
  "$ADB" shell pm clear com.mobile3d.validation >/dev/null
  "$ADB" logcat -c
  "$ADB" shell am start -n com.mobile3d.validation/.MainActivity >/dev/null
  sleep 10
  load=$("$ADB" logcat -d -s Mobile3D:* | grep -oE 'loaded .* in [0-9.]+ ms' | grep -oE '[0-9.]+' | tail -1)
  pss=$("$ADB" shell dumpsys meminfo com.mobile3d.validation | awk '/TOTAL PSS:/{print $3}')
  echo "| $run | ${load:-?} | ${pss:-?} |" >> "$OUT"
  "$ADB" shell am force-stop com.mobile3d.validation
done

{
  echo
  echo "Note: frame/deform stage averages are shown in-app and in logcat (tag Mobile3D)."
  echo "Emulator numbers must not be extrapolated to physical mid-range devices."
} >> "$OUT"

echo "Wrote $OUT"
