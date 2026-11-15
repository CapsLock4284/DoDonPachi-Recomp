"""
Name:        game_profile.py
Description: what the shared decompiler core (tools/core) needs to know of DoDonPachi: where the game is, its ROM set,
             and its settings
Version:     0.1.0 (draft)
Created:     15-11-2026

Modelled on DonPachi Recomp's tools/game_profile.py (DoDonPachi is built on DonPachi's engine: the same frame wait,
frame flag and interrupt entry). Memory map from ddonpach_map in MAME's src/mame/atlus/cave.cpp.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "core"))

import ddonpach_rom as rom  # noqa: E402,F401

ROOT = Path(__file__).resolve().parents[1]
GAME = "DoDonPachi (arcade, Cave 1997)"
ROM_SET_TITLE = {"ddonpach": "DoDonPachi (ddonpach, International, Master Ver. 97/02/05)",
                 "ddonpachj": "DoDonPachi (ddonpachj, Japan, Master Ver. 97/02/05)"}[rom.SET_NAME]
JAPAN = rom.SET_NAME == "ddonpachj"
CPU = "68000"
BINARY = "ddonpach"
GUARD = "DDONPACH"
REFERENCE = None
ANALYSIS = "m68000.analysis"
DISASSEMBLER = "m68000.disassembler"
# 000000-0003FF the vectors, 000400-00041F the version string (still DonPachi's: "DONPACHI Ver1.121995/05/2x XXXXX");
# the first instruction after them is at 000420. The reset vector goes to 0052EA (the Japanese program's 0052F6)
CODE_START = 0x000420

# The board as the program's C sees it (ddonpach_map; an end address is the first byte past)
WORK_RAM_AREA = (0x100000, 0x110000)
PROGRAM_AREA = (0x000000, 0x100000)
HARDWARE = [
    (0x300000, 0x300004, "YMZ280B"),                   # on the low byte of each word: 300001 register, 300003 data
    (0x400000, 0x410000, "SPRITE_RAM"),
    (0x500000, 0x508000, "LAYER0_VRAM"),
    (0x600000, 0x608000, "LAYER1_VRAM"),
    (0x700000, 0x704000, "LAYER2_VRAM"),               # mirrored to 70FFFF
    (0x800000, 0x800080, "VIDEO_REGS"),                # 800000-800007 also tells the interrupt's cause
    (0x900000, 0x900006, "LAYER0_CONTROL"),
    (0xA00000, 0xA00006, "LAYER1_CONTROL"),
    (0xB00000, 0xB00006, "LAYER2_CONTROL"),
    (0xC00000, 0xC10000, "PALETTE"),
    (0xD00000, 0xD00002, "IN0"),
    (0xD00002, 0xD00004, "IN1"),                       # also carries the EEPROM's data out
    (0xE00000, 0xE00001, "EEPROM_CONTROL"),
]
DEVICE_AREAS = [(start, end) for start, end, _ in HARDWARE]
# reading the interrupt cause does something (800004 clears the vblank's, 800006 the other's)
EFFECT_READ_AREAS = [(0x800000, 0x800008)]
REGIONS = [(0xC00000, 0xC10000, "PALETTE_RAM"), (0x400000, 0x410000, "SPRITE_RAM"),
           (0x500000, 0x508000, "LAYER0_RAM"), (0x600000, 0x608000, "LAYER1_RAM"),
           (0x700000, 0x704000, "LAYER2_RAM")]


def here(address):
    """An address of the International program in the program being decompiled (the Japanese: not mapped yet)"""
    if JAPAN:
        raise SystemExit("the Japanese program's addresses are not mapped yet")
    return address


DECOMPILER = "m68000.decompile"
SUPERVISOR_CODE = [(0x000000, 0x100000)]

# the main loop's wait for its next frame, as DonPachi's: 0571AC "addq.b #1, $101273.l", 0571B2 "tst.w $100f44.l"
# (frame_flag, which the interrupt sets), 0571B8 "beq $571ac"; frame_wait() before its first instruction
FRAME_WAIT = 0x0571AC

TIMING_MODEL = True
