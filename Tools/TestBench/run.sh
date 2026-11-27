#!/usr/bin/env bash
# Runs one scripted play session with no one at the keyboard and leaves the log where the
# analyser expects it.  Usage: run.sh <name> [seconds]
# The scenario itself lives in scenarios/scenario.txt, one console command per line, so every run
# gets the identical stimulus and two runs can be compared number for number.
set -u

NAME="${1:-base}"
BUDGET="${2:-115}"
HERE="$(cd "$(dirname "$0")" && pwd)"
SCENARIO="${3:-$HERE/scenarios/scenario.txt}"
LOG="D:/Multiplayer_GAS/Diffs/run_${NAME}.log"
UE="D:/UnrealEngine_5.8_EpicGames/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe"
PROJ="D:/Multiplayer_GAS/Multiplayer_GAS/Multiplayer_GAS.uproject"

# A leftover game process blocks the next compile with "Live Coding is active", and it also
# steals the frame rate from the run we are about to measure.
#
# Only OUR scripted runs, never every UnrealEditor.exe: the editor is also how the user works, on
# this project and on others at the same time, and killing by image name closes their work. The
# filter is this project's path plus -game, which no editor window of theirs matches.
kill_our_runs() {
  powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='UnrealEditor.exe'\" | Where-Object { \$_.CommandLine -like '*Multiplayer_GAS.uproject*' -and \$_.CommandLine -like '*-game*' } | ForEach-Object { Stop-Process -Id \$_.ProcessId -Force }" >/dev/null 2>&1 || true
}
kill_our_runs

# Build first and refuse to run on a failed build: a failed build leaves the previous binary
# in place, the run succeeds, the log looks legitimate, and the conclusions are about code
# that is no longer on disk.  That has cost us a correct fix once already.
#
# SKIP_BUILD=1 for a batch of runs that must share one binary.  Rebuilding inside the batch
# picks up anything edited while it runs, and then the runs being compared are not the same
# program - which has already produced one comparison that had to be thrown away.
if [ "${SKIP_BUILD:-0}" = "1" ]; then
  echo "BUILD: skipped by request"
else
  BUILD=$("D:/UnrealEngine_5.8_EpicGames/UE_5.8/Engine/Build/BatchFiles/Build.bat" \
            Multiplayer_GASEditor Win64 Development -Project="$PROJ" -WaitMutex 2>&1 | tail -40)
  if ! echo "$BUILD" | grep -q "Result: Succeeded"; then
    echo "BUILD FAILED - not running"
    echo "$BUILD" | grep -E "error|Error" | head -20
    exit 1
  fi
  echo "BUILD: Result: Succeeded"
fi

EXEC=$(paste -sd, - < "$SCENARIO" | sed 's/,$//')
echo "SCENARIO: $EXEC"

"$UE" "$PROJ" InitMap -game -windowed -ResX=800 -ResY=600 \
      -log -ABSLOG="$LOG" -ExecCmds="$EXEC" -nosteam &
GAMEPID=$!

# The scripted steps have their own caps, so this is only the backstop that ends the session.
for _ in $(seq 1 "$BUDGET"); do
  kill -0 "$GAMEPID" 2>/dev/null || break
  sleep 1
done
kill -9 "$GAMEPID" 2>/dev/null || true
kill_our_runs
wait "$GAMEPID" 2>/dev/null || true

echo "LOG: $LOG  ($(wc -l < "$LOG" 2>/dev/null || echo 0) lines)"
