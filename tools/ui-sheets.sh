#!/bin/sh
# Contact sheets of the painted screens test_front writes (make host), one
# PNG per size and depth: tools/ui-sheets.sh OUTDIR
out=${1:?usage: ui-sheets.sh OUTDIR}
dir=build/host/front
mkdir -p "$out"
for f in "$dir"/front-*-d*-menu.png; do
	b=$(basename "$f" -menu.png)
	set --
	for s in title menu levels options scores-2 about pause over name text fireworks; do
		[ -f "$dir/$b-$s.png" ] && set -- "$@" "$dir/$b-$s.png"
	done
	montage "$@" -tile 6x -geometry +3+3 -background '#606060' "$out/host-${b#front-}.png"
done
