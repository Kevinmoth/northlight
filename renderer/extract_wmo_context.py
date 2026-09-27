#!/usr/bin/env python3
"""Offline WMO identities; never uses a game process or graphics device."""
import collections
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parent)]
import northlight_paths  # noqa: E402
from mpq import Archive
from extract_signatures import variants, fnv1a64, register
from extract_world_signatures import classify, instructions


def fog_wmo(code):
    """Prove the complete c30 fog equation from model-view Z to FOG output."""
    values={};fog=(4,1) if code[1]<3 else None
    def expression(op,*args):
        if op in (2,5,8,9,10,11):args=tuple(sorted(args,key=repr))
        return (op,*args)
    def read(t):
        if t&0x2000:return [None]*4
        kind,index=register(t)
        raw=values.get((kind,index),[('register',kind,index,c) for c in range(4)])
        result=[raw[(t>>(16+2*c))&3] for c in range(4)]
        modifier=(t>>24)&15
        if modifier:result=[('modifier',modifier,v) for v in result]
        return result
    for op,args in instructions(code):
        if op==31:
            if code[1]==3 and args[0]&31==11 and register(args[1])[0]==6:fog=register(args[1])
            continue
        if op==81:
            if register(args[0])==(2,30):return False
            values[register(args[0])]=list(struct.unpack('<4f',struct.pack('<4I',*args[1:])))
            continue
        if not args:return False
        src=[read(t) for t in args[1:]];result=[None]*4
        if op==1:result=src[0]
        elif op in (8,9):result=[expression(op,tuple(src[0][:3 if op==8 else 4]),tuple(src[1][:3 if op==8 else 4]))]*4
        elif op in (2,4,5,10,11):result=[expression(op,*[s[c] for s in src]) for c in range(4)]
        elif op==32:result=[expression(op,src[0][0],src[1][0])]*4
        dst=register(args[0]);old=values.get(dst,[None]*4)[:]
        for c in range(4):
            if args[0]&(1<<(16+c)):
                old[c]=result[c]
                if args[0]&0x100000:old[c]=expression(10,expression(11,old[c],0.0),1.0)
        values[dst]=old
    z=expression(9,tuple(('register',2,33,c) for c in range(4)),tuple(('register',1,0,c) for c in range(4)))
    affine=expression(4,z,('register',2,30,0),('register',2,30,1))
    expected=expression(10,expression(32,expression(11,affine,0.0),('register',2,30,2)),1.0)
    return fog in values and values[fog][0]==expected


def lit_wmo(code):
    # Deliberately narrow SM3 proof. Unsupported/prelit code is still identified
    # as WMO, but may only obtain lighting from the independent global reader.
    if code[1] != 3:
        return False
    ops = list(instructions(code))
    defined = {register(a[0])[1] for op, a in ops if op == 81}
    if defined.intersection({10, 11, 12, 31, 32, 33}):
        return False
    # Track component provenance through the original straight-line program.
    values = {}
    def source(t):
        kind, number = register(t)
        if t & 0x2000:
            return [None]*4
        value = values.get((kind, number), [('input', kind, number, c) for c in range(4)])
        result = [value[(t >> (16+2*c)) & 3] for c in range(4)]
        if (t >> 24) & 15:
            result = [('modifier', (t >> 24) & 15, x) for x in result]
        return result
    found = False
    for op, a in ops:
        if op in (31, 81):
            continue
        if not a:
            return False
        inputs = [source(t) for t in a[1:]]
        result = [None]*4
        if op == 1:
            result = inputs[0]
        elif op == 8:
            left, right = inputs
            result = [('dot3', tuple(left[:3]), tuple(right[:3]))]*4
        elif op == 36:  # nrm
            result = [('normal', tuple(inputs[0][:3]), c) for c in range(4)]
        elif op == 4:
            for c in range(4):
                scalar, direct, ambient = (value[c] for value in inputs)
                if direct == ('input', 2, 11, c) and ambient == ('input', 2, 10, c):
                    if isinstance(scalar, tuple) and scalar[0] == 'dot3':
                        direction = tuple(('modifier', 1, ('input', 2, 12, k)) for k in range(3))
                        normal = scalar[2] if scalar[1] == direction else scalar[1] if scalar[2] == direction else ()
                        expected = tuple(('dot3', tuple(('input', 2, 31+j, k) for k in range(3)),
                                          tuple(('input', 1, 1, k) for k in range(3))) for j in range(3))
                        if normal == tuple(('normal', expected, j) for j in range(3)):
                            found = True
                result[c] = ('mad', *[value[c] for value in inputs])
        key = register(a[0]);old = values.get(key, [None]*4)[:]
        for c in range(4):
            if a[0] & (1 << (16+c)):
                old[c] = result[c]
        values[key] = old
    return found


def main():
    catalog = json.loads((HERE/'world_shader_signatures.json').read_text())['shaders']
    by_source = collections.defaultdict(set)
    eligible = {}
    for entry in catalog:
        names = [s['path'] for s in entry['sources']]
        if entry['kind'] != 2 or not all(re.search(r'\\MapObj[^\\]+\.bls$', p, re.I) for p in names):
            continue
        eligible[int(entry['hash'], 16)] = entry
        for source in entry['sources']:
            by_source[source['archive']].add(source['path'])
    records = {}
    for archive, names in sorted(by_source.items()):
        with Archive(HERE.parent.parent/archive) as mpq:
            for name in sorted(names):
                for index, _, code in variants(mpq.read(name)):
                    key = fnv1a64(code)
                    if key not in eligible:
                        continue
                    assert classify(code) == 2
                    fog = fog_wmo(code)
                    record = {'hash': f'{key:016x}', 'lit': lit_wmo(code), 'fog': fog,
                              'bytecode_sha256': hashlib.sha256(code).hexdigest(),
                              'source': {'archive': archive, 'path': name, 'variant': index}}
                    if key in records:
                        assert records[key]['lit'] == record['lit'] and records[key]['fog'] == record['fog']
                    records[key] = record
    header = ['// Generated by extract_wmo_context.py; exact WMO-only original shader hashes.',
              '#pragma once', '#include <cstdint>',
              'struct WmoShaderSignature {std::uint64_t hash;bool lighting;bool fog;};',
              'static constexpr WmoShaderSignature kWmoShaderSignatures[] = {']
    for key, record in sorted(records.items()):
        header.append('    {UINT64_C(0x%016x),%s,%s},' % (key, str(record['lit']).lower(), str(record['fog']).lower()))
    header.append('};')
    (northlight_paths.GAMEDATA/'wmo_shader_signatures.h').write_text('\n'.join(header)+'\n')
    report = {'unique_wmo_shaders': len(records), 'lit_proven': sum(r['lit'] for r in records.values()),
              'fog_proven': sum(r['fog'] for r in records.values()), 'shaders': list(records.values())}
    (HERE/'wmo-shader-context.json').write_text(json.dumps(report, indent=2)+'\n')
    print({k:v for k,v in report.items() if k != 'shaders'})


if __name__ == '__main__':
    main()
