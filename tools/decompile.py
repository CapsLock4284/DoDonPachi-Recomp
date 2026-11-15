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
runpy.run_path(str(TOOLS / "core" / "decompile.py"), run_name="__main__")
