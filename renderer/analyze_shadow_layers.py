#!/usr/bin/env python3
"""Attribute F12 shadow coverage to static, terrain, and native model passes.

Read-only/offline. Accept a session directory from runtime-diagnostics-0.3.94.
The sampled counts describe light-map texels, not final screen pixels.
"""
import json
from pathlib import Path
import sys

from analyze_world_diagnostics import Buffer


def static_depth(buffer, x, y, frame, width, height):
    ox, oy, dz = frame['offset']
    cx = x + (buffer.width - width) // 2 + int(ox)
    cy = y + (buffer.height - height) // 2 - int(oy)
    if not (0 <= cx < buffer.width and 0 <= cy < buffer.height):
        return 1.0
    value = buffer.at(cx, cy)[0]
    if value >= 1.0:
        return 1.0
    value += dz
    if frame.get('upstream_retained') and value <= 1:
        return max(value, 0.0)
    return value if 0 <= value <= 1 else 1.0


def analyze(root, capture):
    root = Path(root)
    prefix = f'capture-{capture}-'
    frames = sorted(root.glob(prefix + 's*-c*-frame.json'))
    if not frames:
        raise ValueError('No split shadow frames in this capture')
    first_source = min(json.loads(p.read_text())['source'] for p in frames)
    reports = []
    for path in frames:
        frame = json.loads(path.read_text())
        base = path.name.removesuffix('frame.json')
        static, terrain, live = [Buffer(root / (base + name + '.fgr'))
                                 for name in ('static', 'terrain', 'live')]
        if (terrain.width, terrain.height) != (live.width, live.height):
            raise ValueError('Mismatched scratch sizes')
        final = None
        if frame['source'] == first_source:
            final = Buffer(root / (prefix + 'shadow-' +
                           ('near' if frame['cascade'] == 0 else 'far') + '.fgr'))
        counts = dict(samples=0, native_models_extend_static=0,
                      terrain_extends_static=0, native_models_extend_terrain=0,
                      union_mismatch=0)
        worst = 0.0
        for y in range(2, live.height, 4):
            for x in range(2, live.width, 4):
                s = static_depth(static, x, y, frame, live.width, live.height)
                t, l = terrain.at(x, y)[0], live.at(x, y)[0]
                counts['samples'] += 1
                # One world unit excludes ordinary depth precision differences.
                counts['terrain_extends_static'] += t + 1 / 1280 < s
                counts['native_models_extend_terrain'] += l + 1 / 1280 < t
                counts['native_models_extend_static'] += l + 1 / 1280 < min(s, t)
                if final:
                    delta = abs(final.at(x, y)[0] - min(s, l)) * 1280
                    counts['union_mismatch'] += delta > .01
                    worst = max(worst, delta)
        reports.append(dict(source=frame['source'], cascade=frame['cascade'],
                            geometry=frame['geometry'], signature=frame['signature'],
                            cache_reason=frame['reason'], counts=counts,
                            union_checked=final is not None,
                            worst_union_error_world=worst))
    return dict(capture=capture, layers=reports)


if __name__ == '__main__':
    root = Path(sys.argv[1])
    captures = sorted({int(p.name.split('-')[1]) for p in root.glob('capture-*-s*-frame.json')})
    print(json.dumps([analyze(root, capture) for capture in captures], indent=2, allow_nan=False))
