#!/usr/bin/env python3
# northlight-test: requires=cxx,client,stormlib
"""0.3.161: the analytic blob reference against the tester's own textures\\shadowblob.blp
(decoded from the client MPQ chain, never stored in the repository): mean alpha error 0,
mean colour error < 4, accepted, and the same accept/reject verdict as the real texture
for every test variant. No game, graphics device or Wine."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
from world_scene_builder import Assets,decode_blp
from test_shadow_blob_model import run

assets=Assets()
try:w,h,rgba=decode_blp(assets.read('textures\\shadowblob.blp'),32)
finally:assets.close()
assert (w,h)==(32,32),(w,h)
path=fp.output_dir()/'shadowblob-client-rgba.bin';path.write_bytes(rgba)
try:run(path)
finally:path.unlink()
