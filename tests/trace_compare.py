"""trace_compare.py PORT_TRACE MAME_TRACE SRC_TRACE_DIR: where the port's run of one game frame first goes another way
than the original's. PORT_TRACE: the traced port's block addresses (tests/slowdown_check_trace with TRACE_FRAME=N,
built from tools/decompile.py --trace's C); MAME_TRACE: every instruction of the same frame on MAME's 68000
(tools/cref/cref_timed with TRACE_FRAME=N). The original's instructions are kept where the C has a block, the frame
wait's loop dropped, and the two aligned: the C merges straight-line blocks, so a few blocks only the original
reports are not a difference; the port leaving out a longer run, or going elsewhere, is."""
import difflib
import glob
import re
import sys

WAIT_LOOP = {0x0571AC, 0x0571B2, 0x0571B8, 0x0571BA}

port = [int(line, 16) for line in open(sys.argv[1])]
mame = [int(line, 16) for line in open(sys.argv[2])]
known = set()
for path in glob.glob(sys.argv[3] + "/*.c"):
    for found in re.finditer(r"game_trace(?:_block)?\(0x([0-9A-Fa-f]+)u?\)", open(path).read()):
        known.add(int(found.group(1), 16))


def collapse(sequence):
    out = []
    for address in sequence:
        if address in WAIT_LOOP:
            continue
        if not out or out[-1] != address:
            out.append(address)
    return out


p, q = collapse(port), collapse([a for a in mame if a in known])
shown = 0
for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, p, q, autojunk=False).get_opcodes():
    if tag == "equal" or (tag == "insert" and j2 - j1 <= 8):
        continue
    print(f"{tag}: the port's {' '.join('%06X' % a for a in p[i1:min(i2, i1 + 10)])} | "
          f"the original's {' '.join('%06X' % a for a in q[j1:min(j2, j1 + 10)])} ({j2 - j1} blocks)")
    print("   before:", " ".join("%06X" % a for a in p[max(0, i1 - 10):i1]))
    shown += 1
    if shown >= 6:
        break
if not shown:
    print("the same blocks, in the same order")
