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


_address_map = None
_images = []    # (the two programs: the International one, the one decompiled)


def here(address):
    """An address of the International program (as this file, tools/call_tables.py and symbols.txt give them) in the
    program being decompiled: the same for the International set; for the Japanese, where the same code is
    (tools/version_map.py, DonPachi Recomp's: the two programs aligned, both read from the rom folder). An address in
    code that differs stops the decompile"""
    global _address_map
    if not JAPAN:
        return address
    if _address_map is None:
        sys.path.append(str(Path(__file__).resolve().parent))    # (tools/version_map.py, after the core's own)
        import version_map
        folder = ROOT / "rom"
        _images[:] = [rom.build_program(rom.load_set(folder, "ddonpach")), rom.build_program(rom.load_set(folder))]
        _address_map = version_map.AddressMap(*_images)
    mapped = _address_map(address)
    if mapped is None:
        mapped = _by_neighbours(address)
    if mapped is None:
        raise SystemExit(f"{address:06X} of the International program is in code the Japanese one doesn't share")
    return mapped


def _by_neighbours(address):
    """Where the aligner left an address out though the code is the same (its runs need words found once in each
    program: a routine repeated word for word, as 049DF4's five copies, has none): the shift of a matched run near it
    (the nearest first: in a repeated stretch the aligner may have matched some words to the wrong copy), taken if the
    8 bytes from the address are the same there (its instruction: a call further on may differ, its target moved)"""
    import bisect
    runs, starts = _address_map.runs, _address_map.starts
    word = address >> 1
    at = bisect.bisect_right(starts, word) - 1
    shifts = []
    for index in sorted(range(at - 3, at + 5), key=lambda index: abs(index - at - 0.5)):    # (the nearest first)
        if 0 <= index < len(runs):
            shift = (runs[index][1] - runs[index][0]) * 2
            if shift not in shifts:
                shifts.append(shift)
    world, japan = _images
    for shift in shifts:
        moved = address + shift
        if 0 <= moved and world[address:address + 8] == japan[moved:moved + 8]:
            return moved
    return None


DECOMPILER = "m68000.decompile"
SUPERVISOR_CODE = [(0x000000, 0x100000)]

# the main loop's wait for its next frame, as DonPachi's: 0571AC "addq.b #1, $101273.l", 0571B2 "tst.w $100f44.l"
# (frame_flag, which the interrupt sets), 0571B8 "beq $571ac"; frame_wait() before its first instruction
FRAME_WAIT = here(0x0571AC)

TIMING_MODEL = True
