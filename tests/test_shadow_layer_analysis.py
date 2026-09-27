# northlight-test:
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
import json
from pathlib import Path
import struct
import tempfile
import unittest

from analyze_shadow_layers import analyze, static_depth
from analyze_world_diagnostics import Buffer


class ShadowLayersTest(unittest.TestCase):
    def write_buffer(self, root, name, width, height, value):
        path = root / name
        path.write_bytes(struct.pack('<5I', 0x31524746, width, height, 114, 4) +
                         struct.pack('<f', value) * (width * height))
        return Buffer(path)

    def test_shift_depth_and_clear_sentinel(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            b = self.write_buffer(root, 'map.fgr', 10, 10, .5)
            self.assertAlmostEqual(static_depth(b, 2, 2, {'offset': [1, -1, .1]}, 8, 8), .6)
            self.assertEqual(static_depth(b, 2, 2, {'offset': [-20, 0, 0]}, 8, 8), 1)
            self.assertEqual(static_depth(b, 2, 2, {'offset': [0, 20, 0]}, 8, 8), 1)
            self.assertEqual(static_depth(b, 2, 2, {'offset': [0, 0, -.6]}, 8, 8), 1)
            self.assertEqual(static_depth(b, 2, 2, {'offset': [0, 0, -.6], 'upstream_retained': True}, 8, 8), 0)
            b = self.write_buffer(root, 'map.fgr', 10, 10, 1)
            self.assertEqual(static_depth(b, 2, 2, {'offset': [0, 0, -.3]}, 8, 8), 1)

    def test_pass_attribution_and_union_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            frame = dict(source=0, cascade=0, geometry=2, signature='42',
                         reason='reuse', offset=[0, 0, .1])
            (root / 'capture-1-s0-c0-frame.json').write_text(json.dumps(frame))
            for name, size, depth in [('static', 10, .5), ('terrain', 8, .7), ('live', 8, .4)]:
                self.write_buffer(root, 'capture-1-s0-c0-' + name + '.fgr', size, size, depth)
            self.write_buffer(root, 'capture-1-shadow-near.fgr', 8, 8, .4)
            c = analyze(root, 1)['layers'][0]['counts']
            self.assertEqual(c, dict(samples=4, native_models_extend_static=4,
                                    terrain_extends_static=0, native_models_extend_terrain=4,
                                    union_mismatch=0))
            self.write_buffer(root, 'capture-1-shadow-near.fgr', 8, 8, .8)
            self.assertEqual(analyze(root, 1)['layers'][0]['counts']['union_mismatch'], 4)


if __name__ == '__main__':
    unittest.main()
