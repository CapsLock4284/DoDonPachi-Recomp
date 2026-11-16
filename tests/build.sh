#!/bin/sh
# tests/build.sh: the headless tools (tests/*.c) built with the board (runtime/: Cave68kSDK) and the translated
# program (src/: tools/decompile.py makes it from the ROMs). Objects in tests/bin/obj: delete that folder after the
# runtime or src/ changes (an object is rebuilt only when its .c is newer than it, not its headers)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OBJ="$ROOT/tests/bin/obj"
CFLAGS="-std=gnu11 -O1 -w -fno-strict-aliasing -I$ROOT/include -I$ROOT/runtime/include -I$ROOT/runtime/src -I$ROOT/src"
mkdir -p "$OBJ"
for f in "$ROOT"/runtime/src/*.c "$ROOT"/runtime/src/boards/*.c "$ROOT"/src/*.c "$ROOT"/src/*/*.c; do
	[ -f "$f" ] || continue
	o="$OBJ/$(echo "$f" | sed "s|$ROOT/||;s|/|_|g;s|\.c$|.o|")"
	[ "$o" -nt "$f" ] || { echo "cc $f"; gcc $CFLAGS -c "$f" -o "$o"; }
done
for tool in ${TOOLS:-$(cd "$ROOT/tests" && ls *.c | sed 's/\.c$//')}; do
	gcc $CFLAGS "$ROOT/tests/$tool.c" "$OBJ"/*.o -lm -o "$ROOT/tests/bin/$tool"
	echo "built tests/bin/$tool"
done
