#!/usr/bin/env bash
# run_ryujinx.sh <nro> <seconds> [work dir] [shot-at-seconds...]: boot the nro in the local Ryujinx
# build, headless, on its own data dir (keys copied from ~/.config/Ryujinx or $RYU_KEYS_FROM).
# Guest log lines (svcOutputDebugString) land in <work>/ryu-run.log; the app's sdmc: files in
# <work>/ryudata/sdcard; screenshots of the 1280x720 game area in <work>/shot-<t>.png.
# Xvfb has no DRI3: MESA_VK_WSI_DEBUG=sw lets Ryujinx's Vulkan present anyway (as the MP1
# Remastered rig, mpr-tools/ryu/run.sh, does).
set -euo pipefail
nro=$1 dur=$2 work=${3:-/tmp/opencode/nx-spike}
shift $(( $# < 3 ? $# : 3 ))
ryu=${RYUJINX:-/run/media/odran/Leo/projects/ryujinx/out/Ryujinx}
data=$work/ryudata
if [[ ! -f $data/system/prod.keys ]]; then
  mkdir -p "$data/system"
  cp "${RYU_KEYS_FROM:-$HOME/.config/Ryujinx/system}"/{prod.keys,title.keys} "$data/system/"
fi
display=${RYU_DISPLAY:-:147}
if ! DISPLAY=$display xdpyinfo >/dev/null 2>&1; then
  setsid nohup Xvfb "$display" -screen 0 1920x1080x24 -nolisten tcp >/dev/null 2>&1 </dev/null &
  echo "started Xvfb $display (pid $!); kill it when done"
  read -rt 2 <> <(:) || true
fi
# No WAYLAND_DISPLAY: SDL would open the window on the user's desktop.
env -u WAYLAND_DISPLAY SDL_VIDEODRIVER=x11 MESA_VK_WSI_DEBUG=sw DISPLAY="$display" timeout "$dur" "$ryu" \
  --no-gui --root-data-dir "$data" --skip-user-profiles-manager "$nro" > "$work/ryu-run.log" 2>&1 &
pid=$!
t0=$SECONDS
for at in "$@"; do
  while (( SECONDS - t0 < at )); do read -rt 1 <> <(:) || true; done
  ffmpeg -loglevel error -y -f x11grab -video_size 1920x1080 -i "$display" -frames:v 1 \
    -vf crop=1280:720:320:180 "$work/shot-$at.png" || true
done
wait $pid || true
grep -E "OutputDebugString" "$work/ryu-run.log" | sed 's/.*OutputDebugString: //' | grep -v '^frame' || true
grep -E "OutputDebugString: frame" "$work/ryu-run.log" | tail -1 | sed 's/.*OutputDebugString: //' || true
