"""
Name:        ddonpach_rom.py
Description: MAME's 'ddonpach' ROM set (DoDonPachi, International, Master Ver. 97/02/05), or its Japanese clone
             'ddonpachj' (its own program chips, u27.bin and u26.bin; the rest the parent's) when the environment's
             DDONPACH_SET says so; their regions as the 68000 and the board see them
Version:     0.1.0 (draft)
Created:     15-11-2026

Modelled on DonPachi Recomp's tools/donpachi_rom.py. Files are known by CRC32 + SHA1, not by name, from a folder or
a .zip (merged sets too: each entry is looked at, whatever folder of the .zip it is in).
Regions as ROM_START( ddonpach ) in MAME's src/mame/atlus/cave.cpp (BSD-3-Clause) has them.

Board: Cave 1st generation, ATC03D2: one 68000, no Z80; the sound is one YMZ280B, driven by the 68000 itself, so
there is no sound program.
The program is in two chips, each holding every other byte (ROM_LOAD16_BYTE): u27 the words' high bytes, u26 the
low ones. Checked by the reset vectors: that way round the stack is at 110000 and the start at 0052EA (the Japanese
set's 0052F6); the other way round they are nonsense.
"""
import hashlib
import os
import zipfile
import zlib
from pathlib import Path

SETS = ("ddonpach", "ddonpachj")
SET_NAME = os.environ.get("DDONPACH_SET", "ddonpach")
if SET_NAME not in SETS:
    raise SystemExit(f"DDONPACH_SET is '{SET_NAME}': it can be " + " or ".join(SETS))

# MAME name -> (crc32, sha1, size)
WORLD_FILES = {
    "b1.u27": ("b5cdc8d3", "58757b50e21a27e500a82c03f62cf02a85389926", 0x080000),
    "b2.u26": ("6bbb063a", "e5de64b9c3efc0a38a2e0e16b78ee393bff63558", 0x080000),
    "u50.bin": ("14b260ec", "33bda210302428d5500115d0c7a839cdfcb67d17", 0x200000),
    "u51.bin": ("e7ba8cce", "ad74a6b7d53760b19587c4a6dbea937daa7e87ce", 0x200000),
    "u52.bin": ("02492ee0", "64d9cc64a4ad189a8b03cf6a749ddb732b4a0014", 0x200000),
    "u53.bin": ("cb4c10f0", "a622e8bd0c938b5d38b392b247400b744d8be288", 0x200000),
    "u60.bin": ("903096a7", "a243e903fef7c4a7b71383263e82e42acd869261", 0x200000),
    "u61.bin": ("d89b7631", "a66bb4955ca58fab8973ca37a0f971e9a67ce017", 0x200000),
    "u62.bin": ("292bfb6b", "11b385991ee990eb5ef36e136b988802b5f90fa4", 0x200000),
    "u6.bin": ("9dfdafaf", "f5cb450cdc78a20c3a74c6dac05c9ac3cba08327", 0x200000),
    "u7.bin": ("795b17d5", "cbfc29f1df9600c82e0fdae00edd00da5b73e14c", 0x200000),
    "eeprom-ddonpach.bin": ("315fb546", "7f597107d1610fc286413e0e93c794c80c0c554f", 0x000080),
}
JAPAN_FILES = dict(WORLD_FILES)
del JAPAN_FILES["b1.u27"], JAPAN_FILES["b2.u26"]
JAPAN_FILES["u27.bin"] = ("2432ff9b", "fbc826c30553f6553ead40b312b73c049e8f4bf6", 0x080000)
JAPAN_FILES["u26.bin"] = ("4f3a914a", "ae98eba049f1462aa1145f6959b9f9a32c97278f", 0x080000)
FILES_OF = {"ddonpach": (WORLD_FILES, "b1.u27", "b2.u26"), "ddonpachj": (JAPAN_FILES, "u27.bin", "u26.bin")}
ROM_FILES, HIGH_FILE, LOW_FILE = FILES_OF[SET_NAME]
OPTIONAL_FILES = set()

# The 68000 program image: 000000-0FFFFF, no mirrors (work RAM is at 100000)
PROGRAM_AREAS = [(0x000000, 0x100000)]
PROGRAM_SIZE = 0x100000


def read_files(source):
    """[(entry name, bytes)] from a folder or a .zip (every entry, in any of its folders)"""
    source = Path(source)
    if source.is_file() and source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive:
            return [(entry.filename, archive.read(entry)) for entry in archive.infolist() if not entry.is_dir()]
    return [(path.name, path.read_bytes()) for path in sorted(source.iterdir()) if path.is_file()]


def identify_set(source, set_name=None):
    """({mame_name: bytes}, [report lines]): each file matched by its hashes; a folder's loose files, then its .zips
    for those still missing"""
    files = FILES_OF[set_name or SET_NAME][0]
    by_crc = {crc: name for name, (crc, _, _) in files.items()}
    found, report = {}, []
    sources = [Path(source)]
    if Path(source).is_dir():
        sources += sorted(Path(source).glob("*.zip"))
    for each in sources:
        if set(files) <= set(found):
            break
        for original, data in read_files(each):
            if original.lower().endswith(".zip"):
                continue
            crc = format(zlib.crc32(data) & 0xFFFFFFFF, "08x")
            name = by_crc.get(crc)
            if name and name not in found and files[name][1] == hashlib.sha1(data).hexdigest():
                found[name] = data
                report.append(f"OK      {original:32} = {name}")
    for name in sorted(set(files) - set(found)):
        report.append(f"MISSING {name}")
    return found, report


def load_set(source, set_name=None):
    """identify_set's files; stops when one is missing"""
    set_name = set_name or SET_NAME
    found, _ = identify_set(source, set_name)
    missing = sorted(set(FILES_OF[set_name][0]) - set(found))
    if missing:
        raise SystemExit(f"The '{set_name}' ROM set is incomplete. Missing: " + ", ".join(missing))
    return found


def build_program(rom):
    """maincpu region (1 MB, big endian) as the 68000 sees it at 000000-0FFFFF: ROM_LOAD16_BYTE, the high bytes from
    u27, the low ones from u26"""
    high = rom.get("b1.u27") or rom["u27.bin"]
    low = rom.get("b2.u26") or rom["u26.bin"]
    image = bytearray(2 * len(high))
    image[0::2] = high
    image[1::2] = low
    return bytes(image)


def swap16(data):
    """each 16-bit word's two bytes the other way round"""
    result = bytearray(len(data))
    result[0::2] = data[1::2]
    result[1::2] = data[0::2]
    return bytes(result)


def build_sprites(rom):
    """sprites region (8 MB): u50-u53 one after the other, word swapped as DonPachi's sprites are (the title screen's
    letters read only that way; Cave68kSDK's src/boards/ddonpach.c loads them so)"""
    return b"".join(swap16(rom[name]) for name in ("u50.bin", "u51.bin", "u52.bin", "u53.bin"))


def build_layer0(rom):
    return bytes(rom["u60.bin"])


def build_layer1(rom):
    return bytes(rom["u61.bin"])


def build_layer2(rom):
    """layer 2's tiles: 8x8, 8 bits a pixel"""
    return bytes(rom["u62.bin"])


def build_samples(rom):
    """ymz region (4 MB): the YMZ280B's samples, u6 then u7"""
    return bytes(rom["u6.bin"]) + bytes(rom["u7.bin"])


def build_eeprom(rom):
    return bytes(rom["eeprom-ddonpach.bin"])


def check_vectors(program):
    """The reset vectors: the stack in work RAM, the start an even address in the program. [] when right"""
    problems = []
    stack = int.from_bytes(program[0:4], "big")
    start = int.from_bytes(program[4:8], "big")
    if not 0x100000 <= stack <= 0x110000:
        problems.append(f"initial stack pointer {stack:08X} is not in work RAM (100000-10FFFF): wrong byte order?")
    if not 0x8 <= start < PROGRAM_SIZE or start & 1:
        problems.append(f"reset address {start:08X} is not an even address inside the program: wrong byte order?")
    return problems


def canonical_address(address):
    """A 68000 address's offset in the program image, or None when it is not the ROM (24 address lines)"""
    address &= 0xFFFFFF
    return address if address < PROGRAM_SIZE else None


class ProgramMemory:
    """The program ROM as the 68000 sees it, for the disassembler: big endian, 0xFF outside it"""

    def __init__(self, image):
        self.image = image

    def r8(self, address):
        offset = canonical_address(address)
        return self.image[offset] if offset is not None else 0xFF

    def r16(self, address):
        return (self.r8(address) << 8) | self.r8(address + 1)

    def r32(self, address):
        return (self.r16(address) << 16) | self.r16(address + 2)
