"""
Name:        match_code.py
Description: where DonPachi's code is in DoDonPachi's program: the same instructions (their opcode words: the
             operation, its sizes, modes and registers; not the absolute addresses or long offsets after them), N in a
             row from the address, found once in the other program
Version:     0.1.0 (draft)
Created:     15-11-2026

DoDonPachi is built on DonPachi's engine: much of its code is DonPachi's, moved and with other addresses in it. This
finds where a routine DonPachi's tools name by address went, to carry those settings (and names) over.
Usage: python tools/match_code.py DONPACHI_PROGRAM DODONPACHI_PROGRAM ADDRESS...    (programs as 68000 images)
"""
import sys
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parent / "core")]
from m68000 import disassembler  # noqa: E402

N = 8


class Program:
    def __init__(self, image, start=0x400, end=None):
        self.image = image
        self.memory = disassembler.FlatMemory(image)
        self.end = end or len(image)
        self.lengths = {}
        self.start = start

    def length(self, address):
        if address not in self.lengths:
            instruction = disassembler.decode(self.memory, address)
            self.lengths[address] = 0 if instruction.flow == disassembler.FLOW_INVALID else instruction.length
        return self.lengths[address]

    def shape(self, address, count=N):
        words = []
        for _ in range(count):
            size = self.length(address)
            if not size:
                return None
            words.append(self.image[address] << 8 | self.image[address + 1])
            address += size
        return tuple(words)

    def index(self, count=N):
        found = {}
        for address in range(self.start, self.end - 2, 2):
            key = self.shape(address, count)
            if key:
                found.setdefault(key, []).append(address)
        return found


def matcher(donpachi, dodonpachi, count=N):
    source, target = Program(donpachi), Program(dodonpachi)
    table = target.index(count)

    def find(address):
        key = source.shape(address, count)
        return table.get(key, []) if key else []
    return find, source, target


if __name__ == "__main__":
    first, second = (Path(name).read_bytes() for name in sys.argv[1:3])
    find, _, _ = matcher(first, second)
    for text in sys.argv[3:]:
        address = int(text, 16)
        hits = find(address)
        print(f"{address:06X} -> " + (" ".join(f"{hit:06X}" for hit in hits[:6]) if hits else "none")
              + (f" ({len(hits)} places)" if len(hits) > 1 else ""))
