#!/usr/bin/env python3
"""Build fail-closed shader identities from this client's enabled MPQ archives.

BLS variants retain their original D3D bytecode: hashes cover the complete bytecode
including version and END, excluding the Blizzard container. The union of all
nonempty enabled-archive variants permits both legacy and current shader paths;
empty MPQ deletion markers contribute no shader. It does not assert load order.
"""
from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WORK = HERE.parent
sys.path.insert(0, str(WORK))
import northlight_paths  # noqa: E402
CLIENT = northlight_paths.client_root(required=False) or WORK.parent
from mpq import Archive

PATTERN = re.compile(r'(?:^|\\)(vertex\\vs_[123]_\d+\\Terrain|(?:vertex\\vs_[123]_\d+|pixel\\ps_[123]_\d+)\\UI)\.bls$', re.I)
SM1_LENGTHS = {1: 2, 2: 3, 3: 3, 4: 4, 5: 3, 6: 2, 7: 2, 8: 3, 9: 3, 10: 3, 11: 3, 12: 3, 13: 3, 14: 2, 15: 2, 16: 2, 17: 3, 18: 4, 19: 2, 20: 3, 21: 3, 22: 3, 23: 3, 24: 3, 31: 2, 32: 3, 81: 5}


def fnv1a64(data: bytes) -> int:
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value


def variants(data: bytes):
    if not data:
        return
    if len(data) < 12:
        raise ValueError('Truncated BLS header')
    magic, version, count = struct.unpack_from('<4sII', data)
    if magic not in (b'HSXG', b'SVXG', b'SPXG') or version != 0x10003:
        raise ValueError(f'Unrecognized BLS header: {magic!r}, {version:#x}')
    offset = 12
    for index in range(count):
        if offset + 16 > len(data):
            raise ValueError('Truncated variant header')
        flags = struct.unpack_from('<4I', data, offset)
        size = flags[3]
        offset += 16
        code = data[offset:offset + size]
        if size == 0:  # Absent shader permutation; header still occupies 16 bytes.
            continue
        if size < 8 or size % 4 or len(code) != size:
            raise ValueError(f'Invalid variant size: {size}')
        token, = struct.unpack_from('<I', code)
        if token >> 16 not in (0xfffe, 0xffff) or code[-4:] != b'\xff\xff\x00\x00':
            raise ValueError('Invalid D3D shader bytecode bounds')
        offset += size
        yield index, flags[:3], code
    if offset != len(data):
        raise ValueError(f'Unexpected trailing BLS bytes: {len(data) - offset}')


def position_prefix(code: bytes):
    """Decode only through the first full position write, rejecting unknown ops."""
    words = struct.unpack(f'<{len(code) // 4}I', code)
    major = (words[0] >> 8) & 255
    offset = 1
    result = []
    position_register = (4, 0) if major < 3 else None
    while offset < len(words):
        token = words[offset]
        op = token & 65535
        if op == 65535:
            break
        if op == 65534:
            offset += 1 + ((token >> 16) & 32767)
            continue
        size = (token >> 24) & 15 if major >= 2 else SM1_LENGTHS[op]
        args = tuple(words[offset + 1:offset + 1 + size])
        if len(args) != size:
            raise ValueError('Truncated instruction')
        offset += 1 + size
        if op == 31:  # dcl_position o# for SM3
            if major == 3 and args[0] & 31 == 0 and register(args[1])[0] == 6:
                position_register = register(args[1])
            continue
        if op == 81:  # def constant, unrelated to matrix register uploads
            continue
        # Commutative MUL operand reversal is the only compiler variation here.
        if op == 5:
            args = (args[0], *sorted(args[1:]))
        result.append((op, args))
        if register(args[0]) == position_register and ((args[0] >> 16) & 15) == 15:
            # Canonicalize SM1/2 oPos0 and SM3 declared position output.
            result[-1] = (op, (0xc00f0000, *args[1:]))
            return tuple(result)
    raise ValueError('No full vertex position output found')


def register(token: int):
    return ((token >> 28) & 7) | ((token >> 8) & 24), token & 2047


# This exact instruction chain proves the matrix register range and orientation.
# r0 = input.x*c0 + input.y*c1 + input.z*c2 + c3
# clip = r0.x*c4 + r0.y*c5 + r0.z*c6 + r0.w*c7
EXPECTED_POSITION = (
    (5, (0x800f0000, *sorted((0x90550000, 0xa0e40001)))),
    (4, (0x800f0000, 0x90000000, 0xa0e40000, 0x80e40000)),
    (4, (0x800f0000, 0x90aa0000, 0xa0e40002, 0x80e40000)),
    (2, (0x800f0000, 0x80e40000, 0xa0e40003)),
    (5, (0x800f0001, *sorted((0x80550000, 0xa0e40005)))),
    (4, (0x800f0001, 0x80000000, 0xa0e40004, 0x80e40001)),
    (4, (0x800f0001, 0x80aa0000, 0xa0e40006, 0x80e40001)),
    (4, (0xc00f0000, 0x80ff0000, 0xa0e40007, 0x80e40001)),
)


def main():
    inventory = json.loads((WORK / 'inspection/inventory.json').read_text())
    records = {name: {} for name in ('TerrainVS', 'UiVS', 'UiPS')}
    empty = []
    variant_count = 0
    for path, entry in sorted(inventory.items()):
        if not entry['enabled']:
            continue
        listing = WORK / 'inspection' / (Path(path).name + '.list')
        selected = sorted(n for n in listing.read_text().splitlines() if PATTERN.search(n))
        if not selected:
            continue
        with Archive(CLIENT / path) as archive:
            for name in selected:
                data = archive.read(name)
                if not data:
                    empty.append({'archive': path, 'path': name})
                    continue
                category = 'TerrainVS' if name.lower().endswith('terrain.bls') else ('UiVS' if '\\vertex\\' in name.lower() else 'UiPS')
                for index, flags, code in variants(data):
                    variant_count += 1
                    value = fnv1a64(code)
                    if category == 'TerrainVS':
                        prefix = position_prefix(code)
                        if prefix != EXPECTED_POSITION:
                            raise ValueError(f'Unrecognized terrain matrix path: {path}:{name}[{index}] {prefix!r}')
                    existing = records[category].get(value)
                    if existing is not None and existing['bytecode'] != code.hex():
                        raise ValueError('FNV64 collision')
                    row = records[category].setdefault(value, {'hash': f'{value:016x}', 'bytes': len(code), 'bytecode': code.hex(), 'sources': []})
                    row['sources'].append({'archive': path, 'path': name, 'variant': index, 'flags': flags})
    if set(records['TerrainVS']) & set(records['UiVS']):
        raise ValueError('Terrain and UI vertex identities overlap')
    report = {
        'algorithm': 'FNV-1a-64 over complete original D3D bytecode bytes',
        'source_policy': 'Union of nonempty variants in enabled archive inventory; no claim about archive priority.',
        'variant_count': variant_count,
        'unique_counts': {k: len(v) for k, v in records.items()},
        'ignored_empty_entries': empty,
        'terrain_projection': {
            'first_register': 4,
            'register_count': 4,
            'matrix_layout': 'Rows, row-vector multiplication: clip = view.x*c4 + view.y*c5 + view.z*c6 + view.w*c7',
            'proof': 'Every included terrain variant has the exact validated eight-instruction position chain.',
            'inference': 'c0..3 is modelview and c4..7 projection; camera-space z is subsequently used for fog. Validate pure perspective matrix form at runtime.',
            'depth_formula': 'For standard perspective: A=c6.z, B=c7.z, C=c6.w; viewZ=B/(depth*C-A). near=abs(-B/A); far=abs(B/(C-A)).',
            'required_validation': 'Reject nonfinite values, C near zero, c7.w nonzero, nonzero off-diagonal projection terms except optional c6.xy; near/far invalid. Off-center projection requires offsets in reconstruction.',
        },
        'shaders': {k: [{a: b for a, b in row.items() if a != 'bytecode'} for _, row in sorted(v.items())] for k, v in records.items()},
    }
    (HERE / 'signatures.json').write_text(json.dumps(report, indent=2) + '\n')
    header = ['// Generated by extract_signatures.py; do not edit.', '#pragma once', '#include <cstdint>', '', '// Terrain matrix registers are rows used by row-vector multiplication.', 'static constexpr unsigned kTerrainProjectionRegister = 4;', '']
    for category, rows in records.items():
        header.append(f'static constexpr std::uint64_t k{category}[] = {{')
        header.extend(f'    UINT64_C(0x{value:016x}),' for value in sorted(rows))
        header.extend(('};', ''))
    (northlight_paths.GAMEDATA / 'signatures.h').write_text('\n'.join(header))
    print(json.dumps({'variant_count': variant_count, 'unique_counts': report['unique_counts'], 'matrix_verified': True, 'empty_entries': len(empty)}))


if __name__ == '__main__':
    main()
