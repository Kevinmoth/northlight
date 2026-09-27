#!/usr/bin/env python3
# northlight-test:
"""scripts/world_cache_diff.py on two synthetic caches: an identical copy and float noise
under the tolerance are equal; each kind of change lands in its own category."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
import hashlib, shutil, struct
import world_cache_diff as wcd


def model(vertices, material=b'\x10' * 4):
    blob = struct.pack('<4s4I', b'FGS2', 2, len(vertices), 1, 1)
    blob += b''.join(struct.pack('<8f', *v, 0, 0, 1, 0, 0) for v in vertices)
    blob += struct.pack('<4I', 0, 1, 2, 0) + struct.pack('<3fIIfI', 1, 1, 1, 1, 1, 0, 4) + material
    return blob


def cache(root, shift=0.0, doodad=True, material=b'\x10' * 4, apex=1.0, light=1.0, zone=12):
    shutil.rmtree(root, ignore_errors=True)
    (root / 'Azeroth').mkdir(parents=True)
    (root / 'models').mkdir()
    keys = [hashlib.sha256(n).digest() for n in (b'terrain', b'tree', b'lamp')]
    (root / 'models' / (keys[0].hex() + '.fgs')).write_bytes(model([(0, 0, 0), (1, 0, 0), (0, 1, 0)]))
    (root / 'models' / (keys[1].hex() + '.fgs')).write_bytes(model([(0, 0, 0), (1, 0, 0), (0, 1, apex)], material))
    (root / 'models' / (keys[2].hex() + '.fgs')).write_bytes(model([(0, 0, 0), (2, 0, 0), (0, 2, 0)]))
    ident = (1, 0, 0, 0, 1, 0, 0, 0, 1)
    rows = [(keys[0], (32 << 16) | 49, 0, -9000, 0, 0, -8500, 500, 100, *ident, 0, 0, 0),
            (keys[1], 7, 1, -8800, 100, 0, -8799, 101, 1, *ident, -8800 + shift, 100, 0)]
    if doodad:
        rows.append((keys[2], (5 << 32) | 3, 3, -8700, 200, 0, -8698, 202, 0, *ident, -8700, 200, 0))
    (root / 'Azeroth/32_49.fg3').write_bytes(struct.pack('<4sII', b'FGS3', 3, len(rows))
                                             + b''.join(struct.pack('<32sQI18f', *r) for r in rows))
    (root / 'lights').mkdir()
    lights = [(-8700, 200, 5, light, 1, 1, 2, 8, 0x1234, 1, 0)]
    (root / 'lights/Azeroth.fgl').write_bytes(struct.pack('<4sIII', b'FGL1', 1, 48, 1)
                                              + b''.join(struct.pack('<8fQII', *l) for l in lights))
    (root / 'fog/Azeroth').mkdir(parents=True)
    (root / 'fog/Azeroth/32_49.frf').write_bytes(struct.pack('<4s5I', b'FRF1', 1, 32, 49, 1, 0)
                                                 + struct.pack('<256I', *([zone] * 256)) + struct.pack('<3fQ', -8600, 300, 10, 255))
    (root / 'celestial').mkdir()
    (root / 'celestial/moon.fct').write_bytes(b'FCT1' + material)
    return root


out = fp.output_dir()
a = cache(out / 'a')


def check(expect, **change):
    result = wcd.diff(a, cache(out / 'b', **change), ['Azeroth'], [('Azeroth', 32, 49, 32, 49)], 1e-3, 64.0)
    unequal = {name: result[name]['counts'] for name in ('tiles', 'instances', 'models', 'lights', 'fog', 'celestial')
               if not result[name]['equal']}
    got = {name: sorted(k for k in counts if not k.startswith(('equal', 'identical', 'both', 'outside'))) for name, counts in unequal.items()}
    assert got == expect, (change, got)


check({})                                    # identical copy
check({}, shift=0.004)                       # float32 noise at x = -8800 (< 4 ulps) is equal
check({'instances': ['transform:m2']}, shift=0.05)
check({'instances': ['only_a:wmo_doodad']}, doodad=False)
check({'models': ['materials:m2'], 'celestial': ['different']}, material=b'\x20' * 4)
check({'models': ['geometry:m2']}, apex=3.0)
check({'lights': ['values']}, light=0.5)
check({'fog': ['zones']}, zone=40)
print('world_cache_diff: equal copy, tolerance and every category OK')
