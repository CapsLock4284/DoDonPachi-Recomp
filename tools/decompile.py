"""
Name:        decompile.py
Description: runs the shared core's decompile.py (tools/core) for DoDonPachi: the 68000 program as readable C in src/
Version:     0.1.0 (draft)
Created:     15-11-2026

The core is arcade-decomp (tools/core); tools/game_profile.py tells it about this game. Usage as the core's:
  python tools/decompile.py            src/
  python tools/decompile.py --check    what does not lift
"""
import runpy
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]
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
sys.exit(outcome[0] if outcome else 1)
