#!/bin/sh
# tests/build.sh: the headless tools (tests/*.c) built with the board (runtime/: Cave68kSDK) and the translated
# program (src/: tools/decompile.py makes it from the ROMs). Objects in tests/bin/obj: delete that folder after the
# runtime or src/ changes (an object is rebuilt only when its .c is newer than it, not its headers)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OBJ="$ROOT/tests/bin/obj"
CFLAGS="-std=gnu11 -O1 -w -fno-strict-aliasing -I$ROOT/include -I$ROOT/runtime/include -I$ROOT/runtime/src -I$ROOT/src"
# the Japanese program too, when tools/decompile.py --set japan has made src_jpn/ (its names as jpn_NAME, as
# CMakeLists.txt builds it): the runtime then runs either, by the ROM set's version (VERSION=1 in the tools)
JPN="$ROOT/src_jpn"
[ -f "$JPN/jpn_names.h" ] && CFLAGS="$CFLAGS -DDONPACHI_JAPAN_PROGRAM"
mkdir -p "$OBJ"
for f in "$ROOT"/runtime/src/*.c "$ROOT"/runtime/src/boards/*.c "$ROOT"/src/*.c "$ROOT"/src/*/*.c; do
	[ -f "$f" ] || continue
	o="$OBJ/$(echo "$f" | sed "s|$ROOT/||;s|/|_|g;s|\.c$|.o|")"
	[ "$o" -nt "$f" ] || { echo "cc $f"; gcc $CFLAGS -c "$f" -o "$o"; }
done
if [ -f "$JPN/jpn_names.h" ]; then
	for f in "$JPN"/*.c "$JPN"/*/*.c; do
		[ -f "$f" ] || continue
		o="$OBJ/$(echo "$f" | sed "s|$ROOT/||;s|/|_|g;s|\.c$|.o|")"
		[ "$o" -nt "$f" ] || { echo "cc $f"; gcc -std=gnu11 -O1 -w -fno-strict-aliasing -I$ROOT/include \
			-I$ROOT/runtime/include -I"$JPN" -include "$JPN/jpn_names.h" -c "$f" -o "$o"; }
	done
fi
for tool in ${TOOLS:-$(cd "$ROOT/tests" && ls *.c | sed 's/\.c$//')}; do
	gcc $CFLAGS "$ROOT/tests/$tool.c" "$OBJ"/*.o -lm -o "$ROOT/tests/bin/$tool"
	echo "built tests/bin/$tool"
done
