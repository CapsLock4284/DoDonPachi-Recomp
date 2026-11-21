"""
Name:        decompile.py
Description: runs the shared core's decompile.py (tools/core) for DoDonPachi: the 68000 program as readable C in src/
Version:     0.1.0 (draft)
Created:     15-11-2026

The core is arcade-decomp (tools/core); tools/game_profile.py tells it about this game. Usage as the core's:
  python tools/decompile.py            src/
  python tools/decompile.py --check    what does not lift
and --set japan for the Japanese version ('ddonpachj', its program chips in the rom folder's set: a merged
ddonpach.zip has them, or ddonpachj.zip beside it):
  python tools/decompile.py --set japan    src_jpn/
As DonPachi Recomp's: symbols.txt's names are the International program's; for the Japanese one they are carried over
to where the same code is (tools/version_map.py) into build/decompile_jpn/symbols_jpn.txt (made each time), those in
code the Japanese program doesn't share left out (listed there); tools/call_tables.py's addresses the same way
(game_profile.here). src_jpn/jpn_names.h then names every name its C defines jpn_NAME, so both programs build into
one game (CMakeLists.txt).
"""
import os
import runpy
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

SETS = {"world": "ddonpach", "ddonpach": "ddonpach", "japan": "ddonpachj", "ddonpachj": "ddonpachj"}
if "--set" in sys.argv:
    position = sys.argv.index("--set")
    if position + 1 >= len(sys.argv) or sys.argv[position + 1].lower() not in SETS:
        raise SystemExit("--set world or --set japan")
    os.environ["DDONPACH_SET"] = SETS[sys.argv[position + 1].lower()]
    del sys.argv[position:position + 2]
JAPAN = os.environ.get("DDONPACH_SET") == "ddonpachj"


def carry_symbols(source, target, here):
    """symbols.txt's lines with their program addresses moved to the Japanese program's (here(): None where the code
    differs, the line left out); the work RAM's (var) as they are. The lines left out"""
    lines, left_out = [], []
    for line in source.read_text(encoding="utf-8").splitlines():
        parts = line.split("#", 1)[0].split()
        if len(parts) >= 3 and parts[1] in ("func", "code", "data", "file") and all(
                c in "0123456789abcdefABCDEF" for c in parts[0]):
            moved = here(int(parts[0], 16))
            if moved is None:
                left_out.append(line)
                lines.append(f"# (not in the Japanese program) {line}")
                continue
            start = line.index(parts[0])
            line = line[:start] + f"{moved:06X}" + line[start + len(parts[0]):]
        lines.append(line)
    header = ["# Made by tools/decompile.py --set japan from symbols.txt (the International program's names), each",
              "# address moved to where the Japanese program has the same code (tools/version_map.py). The comments'",
              f"# addresses are still the International program's. Left out, in code the Japanese program doesn't share: "
              f"{len(left_out)}", ""]
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(header + lines) + "\n", encoding="utf-8")
    return left_out


def prefix_header(out_dir):
    """out_dir/jpn_names.h: every name the Japanese program's C defines (its functions, the tables' copies, the
    registry: game_code, game_entry, game_tables...), as jpn_NAME (DonPachi Recomp's way). The build includes it before
    each of its files, so the two programs' C goes into one game without a name twice; the runtime picks the program
    by the ROM set (runtime/src/game_program.c). Found as the C has them: what starts at a line's first column and is
    defined there (not static, typedef or extern)"""
    import re
    names = set()
    definition = re.compile(r"(?:const\s+)?(?:unsigned\s+)?[A-Za-z_]\w*(?:\s+const)?[\s*]+(?:const\s+)?\**\s*"
                            r"([A-Za-z_]\w*)\s*(\(|\[|=|;)")
    for path in sorted(out_dir.rglob("*.c")):
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if not line or line[0] in " \t#/{}*" or line.startswith(
                    ("static", "typedef", "extern", "struct ", "enum ", "union ")):
                continue
            found = definition.match(line)
            if found:
                names.add(found.group(1))
    lines = ["/* The Japanese program's names, each as jpn_NAME: included before each file of src_jpn/ (made by",
             "   tools/decompile.py --set japan), so that its C and the International program's go into one game */",
             "#ifndef DDONPACH_JPN_NAMES_H", "#define DDONPACH_JPN_NAMES_H", ""]
    lines += [f"#define {name} jpn_{name}" for name in sorted(names)]
    lines += ["", "#endif", ""]
    (out_dir / "jpn_names.h").write_text("\n".join(lines), encoding="ascii")
    print(f"prefix:     {out_dir / 'jpn_names.h'} ({len(names)} names, jpn_)")


if JAPAN:
    import game_profile  # noqa: E402
    import m68000.decompile  # noqa: E402

    reports = ROOT / "build" / "decompile_jpn"
    symbols = reports / "symbols_jpn.txt"
    game_profile.here(0x000420)    # (the two programs aligned)
    left_out = carry_symbols(ROOT / "symbols.txt", symbols, game_profile._address_map)
    print(f"symbols:    {symbols} (symbols.txt's names moved; {len(left_out)} in code that differs, left out)")
    m68000.decompile.REPORTS = reports
    if "--symbols" not in sys.argv:
        sys.argv += ["--symbols", str(symbols)]
    if "--out" not in sys.argv:
        sys.argv += ["--out", str(ROOT / "src_jpn")]

import call_tables  # noqa: E402

call_tables.install()

# DoDonPachi has expressions nested deeper than Python's default limits allow (the C writer's simplify pass recursed
# past 1,000 levels): the core runs in a thread with a large stack, its recursion limit raised to match
import threading  # noqa: E402

outcome = []


def run():
    try:
        runpy.run_path(str(TOOLS / "core" / "decompile.py"), run_name="__main__")
        outcome.append(0)
    except SystemExit as stopped:
        outcome.append(stopped.code)
    except BaseException as error:  # noqa: BLE001 (shown, then the program ends with it)
        import traceback
        traceback.print_exc()
        outcome.append(1)


# a function the C writer cannot structure (structure.py's ValueError "block end ...") is listed with the others it
# cannot write, as an EmitError, instead of stopping the whole run: one run shows them all
from decompiler import emit as _emit, structure as _structure  # noqa: E402

_structured = _structure.structured


def _listed(function, *rest, **named):
    try:
        return _structured(function, *rest, **named)
    except ValueError as problem:
        raise _emit.EmitError(f"cannot structure: {problem}") from None


_structure.structured = _listed

sys.setrecursionlimit(200_000)
threading.stack_size(1024 * 1024 * 1024)
worker = threading.Thread(target=run)
worker.start()
worker.join()
if outcome and outcome[0] in (0, None) and not any(arg.startswith("--check") and arg != "--checks"
                                                   for arg in sys.argv[1:]):
    # an interrupt over the bytes some routines keep below their stack pointer (call_tables.py: STACK_DIGITS)
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else str(TOOLS.parent / "src")
    call_tables.interrupt_frames(out)
    if JAPAN:
        prefix_header(Path(out))
sys.exit(outcome[0] if outcome else 1)
