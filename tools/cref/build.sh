#!/bin/sh
# tools/cref/build.sh: tools/cref/cref_timed (DoDonPachi's original program on MAME's 68000 core, at the board's
# timing) into tests/bin. MAME's 68000 (tools/core/konami/reference/mame/m68000, BSD-3-Clause): its handlers written
# by its m68000gen.py from m68000.lst, as MAME's build does; the board from runtime/ (Cave68kSDK). Linux or MSYS2:
# g++ (C++20) and Python 3
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
REF="$ROOT/tools/core/konami/reference"
M68K="$REF/mame/m68000"
OUT="$ROOT/tests/bin/cref"
mkdir -p "$OUT/generated"
for part in header:m68000-head.h decode:m68000-decode.cpp sdf:m68000-sdf.cpp sdp:m68000-sdp.cpp; do
	[ -f "$OUT/generated/${part#*:}" ] || python3 "$M68K/m68000gen.py" "${part%%:*}" "$M68K/m68000.lst" "$OUT/generated/${part#*:}"
done
CXX="g++ -std=c++20 -O1 -w -I$REF/stub -I$M68K -I$OUT/generated -I$ROOT/runtime/include"
for f in "$M68K/m68000.cpp" "$M68K/m68kdasm.cpp" "$REF/m68000_spaces.cpp" "$OUT/generated/m68000-decode.cpp" \
         "$OUT/generated/m68000-sdf.cpp" "$OUT/generated/m68000-sdp.cpp"; do
	o="$OUT/$(basename "$f" .cpp).o"
	[ -f "$o" ] || { echo "c++ $f"; $CXX -c "$f" -o "$o"; }
done
for f in "$ROOT"/runtime/src/machine.c "$ROOT"/runtime/src/rom_set.c "$ROOT"/runtime/src/inflate.c \
         "$ROOT"/runtime/src/sound.c "$ROOT"/runtime/src/boards/*.c; do
	gcc -std=gnu11 -O2 -w -I"$ROOT/runtime/include" -I"$ROOT/runtime/src" -c "$f" -o "$OUT/board_$(basename "$f" .c).o"
done
$CXX -c "$ROOT/tools/cref/cref_timed.cpp" -o "$OUT/cref_timed.o"
g++ "$OUT"/*.o -lm -o "$ROOT/tests/bin/cref_timed"
echo "built tests/bin/cref_timed"
