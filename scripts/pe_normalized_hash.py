#!/usr/bin/env python3
"""SHA-256 of a PE image with the path-derived build-ID bytes zeroed.

lld derives the COFF TimeDateStamp, the debug-directory timestamps and the
CodeView (RSDS) GUID from a hash of the PDB, and the PDB records absolute
source/object paths. Everything else in frd9.dll is path-independent, so two
builds of the same sources from different directories match here.
"""
import hashlib, struct, sys

def normalized(data):
    d = bytearray(data)
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    assert d[pe:pe+4] == b'PE\0\0'
    struct.pack_into('<I', d, pe + 8, 0)                       # COFF TimeDateStamp
    opt = pe + 24
    magic = struct.unpack_from('<H', d, opt)[0]
    dirs = opt + (96 if magic == 0x10b else 112)
    struct.pack_into('<I', d, opt + 64, 0)                     # CheckSum
    rva, size = struct.unpack_from('<II', d, dirs + 6 * 8)     # IMAGE_DIRECTORY_ENTRY_DEBUG
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    sec = opt + struct.unpack_from('<H', d, pe + 20)[0]
    def off(r):
        for i in range(nsec):
            vs, va, rs, rp = struct.unpack_from('<IIII', d, sec + 40 * i + 8)
            if va <= r < va + max(vs, rs): return rp + r - va
        raise ValueError(hex(r))
    if size:
        base = off(rva)
        for e in range(size // 28):
            ent = base + 28 * e
            struct.pack_into('<I', d, ent + 4, 0)              # entry TimeDateStamp
            typ, dsize, _, raw = struct.unpack_from('<IIII', d, ent + 12)
            if typ == 2 and d[raw:raw+4] == b'RSDS':
                d[raw+4:raw+24] = bytes(20)                    # GUID + Age
            elif typ == 16:
                d[raw:raw+dsize] = bytes(dsize)                # REPRO hash
    return bytes(d)

if __name__ == '__main__':
    for p in sys.argv[1:]:
        raw = open(p, 'rb').read()
        print(hashlib.sha256(normalized(raw)).hexdigest(), hashlib.sha256(raw).hexdigest()[:16], p)
