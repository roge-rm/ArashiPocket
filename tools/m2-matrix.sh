#!/bin/sh
# Runs the test pattern on each given matrix device (default: all with
# firmware), one after another, and keeps m2/DEVICE.png.
# Usage: tools/m2-matrix.sh [DEVICE...]   (OUT=folder to change the output)
here=$(dirname "$0")
out=${OUT:-/srv/downloads/temp/debug/ArashiPocket/m2}
devs=${*:-iiix m100 vx iiic m505 t415 tg50 e2 tx zodiac dana}
for d in $devs; do
  rm -rf "$out/run-$d"
  speed=; case $d in iiix|m100|vx|iiic|m505|t415|dana) speed="--speed uncapped";; esac
  echo "== $d"
  python3 -I "$here/frond-device.py" "$d" "$out/run-$d" --replay "$here/replays/m2-pen.replay" \
    --seconds 4 $speed 2>&1 | tail -3
  [ -f "$out/run-$d/end.png" ] && cp "$out/run-$d/end.png" "$out/$d.png"
done
