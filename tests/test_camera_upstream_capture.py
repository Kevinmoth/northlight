#!/usr/bin/env python3
# northlight-test: manual  (needs a capture directory argument)
"""Offline regression from the 0.3.94 Duskwood camera captures 3/4.

The counterfactual changes only cached-depth handling. It does not simulate
fresh GPU rasterization or claim a complete visual acceptance test.
"""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
import collections
import json
from pathlib import Path
import sys

from analyze_world_diagnostics import Buffer


def compare(root):
    report = []
    for cascade in (0, 1):
        a, b = [json.loads((root / f'capture-{i}-s0-c{cascade}-frame.json').read_text())
                for i in (3, 4)]
        assert a['cached'] == b['cached'] and a['geometry'] == b['geometry']
        assert a['working'][:12] == b['working'][:12]
        paths = [root / f'capture-{i}-s0-c{cascade}-static.fgr' for i in (3, 4)]
        assert paths[0].read_bytes() == paths[1].read_bytes()
        dx = round((b['working'][12] - a['working'][12]) * 512)
        dy = round(-(b['working'][13] - a['working'][13]) * 512)
        dz = b['working'][14] - a['working'][14]
        static = Buffer(paths[0])
        live = [Buffer(root / f'capture-{i}-s0-c{cascade}-live.fgr') for i in (3, 4)]
        counts = collections.Counter()
        for y in range(4, 1020, 4):
            for x in range(4, 1020, 4):
                xx, yy = x + dx, y + dy
                if not (0 <= xx < 1024 and 0 <= yy < 1024):
                    continue
                c = static.at(x + 128 + a['offset'][0], y + 128 - a['offset'][1])[0]
                for mode in ('old', 'retained_upstream'):
                    values = []
                    for j, frame in enumerate((a, b)):
                        value = c + frame['offset'][2]
                        if c >= 1 or value > 1:
                            depth = 1
                        elif mode == 'retained_upstream':
                            depth = max(0, value)
                        else:
                            depth = value if value >= 0 else 1
                        values.append(min(depth, live[j].at(x if j == 0 else xx, y if j == 0 else yy)[0]))
                    # Zero encodes upstream occlusion; compare that sentinel
                    # directly. Exact non-clamped depths carry the origin shift.
                    old = values[0] + dz if 0 < values[0] < 1 else values[0]
                    counts[mode] += abs(old - values[1]) * 1280 > 1
                counts['samples'] += 1
        assert counts['old'] > 1000
        assert counts['retained_upstream'] < counts['old'] * .2
        if cascade == 0:
            assert counts['retained_upstream'] < 20
        report.append(dict(cascade=cascade, static_identical=True, counts=dict(counts)))
    return report


if __name__ == '__main__':
    print(json.dumps(compare(Path(sys.argv[1])), indent=2))
