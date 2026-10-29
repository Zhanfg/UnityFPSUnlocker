#!/bin/sh
# Simulate multiple kernel interface layouts without a phone or root.
set -eu
cd "$(dirname "$0")/.."
script="packaging/fengchi-probe.sh"
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT HUP INT TERM

out=$(FENGCHI_PROBE_ROOT="$root" sh "$script")
printf '%s\n' "$out" | grep -qx 'classification=unknown'
printf '%s\n' "$out" | grep -qx 'oem_superframe_max_fps=unknown'

mkdir -p "$root/proc/hmbird_sched" "$root/proc/game_opt/frame_sync" "$root/proc/game_opt/early_detect"
: > "$root/proc/game_opt/geas_ctrl"
out=$(FENGCHI_PROBE_ROOT="$root" sh "$script")
for expected in 'hmbird=1' 'game_opt=1' 'frame_sync=1' 'geas=1' 'early_detect=1' 'classification=oem_game_stack_candidate'; do
  printf '%s\n' "$out" | grep -qx "$expected"
done

rm -rf "$root/proc"
mkdir -p "$root/sys/module/oplus_game" "$root/sys/module/oplus_geas"
out=$(FENGCHI_PROBE_ROOT="$root" sh "$script")
printf '%s\n' "$out" | grep -qx 'classification=oem_game_stack_candidate'
printf '%s\n' "$out" | grep -qx 'hmbird=0'

rm -rf "$root/sys"
mkdir -p "$root/proc/hmbird_sched"
out=$(FENGCHI_PROBE_ROOT="$root" sh "$script")
printf '%s\n' "$out" | grep -qx 'classification=partial_interfaces'
printf '%s\n' "$out" | grep -qx 'game_opt=0'
echo 'Fengchi cross-layout read-only probe tests passed'
