#!/usr/bin/env python3

"""Total size of every ALLOC section in an ELF file.

Reads the section table of an ELF file and sums the sizes of all sections that 
have the ALLOC flag set.
"""
import re, subprocess, sys

SECTION = re.compile(r"\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+([0-9a-fA-F]+)\s+"
                     r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+(\S*)")

def footprint(binary: str) -> int:
    out = subprocess.check_output(["readelf", "-S", "-W", binary], text=True)
    total, seen = 0, False
    for line in out.splitlines():
        m = SECTION.match(line)
        if not m:
            continue
        seen = True
        if "A" in m.group(7):
            total += int(m.group(5), 16)
    if not seen:
        raise RuntimeError("no section table in %s" % binary)
    return total

if __name__ == "__main__":
    print(footprint(sys.argv[1]))
