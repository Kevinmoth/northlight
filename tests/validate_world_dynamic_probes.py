#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native deterministic transport/cache regression; never starts a game."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def main():
    sources = ['world_dynamic_probes.h', 'test_world_dynamic_probes.cpp', 'test_world_moving_replay.cpp',
               'world_probe_cache.h', 'world_gi.h', 'world_gi.cpp']
    with tempfile.TemporaryDirectory(prefix='northlight-dynamic-probes-') as tmp:
        binary = Path(tmp) / 'test'
        command = ['clang++', '-std=c++17', '-O1', '-g',
                   '-fsanitize=address,undefined',
                   *fp.test_include_flags(), str(HERE / 'test_world_dynamic_probes.cpp'),
                   str(fp.src('world_gi.cpp')), '-o', str(binary)]
        subprocess.run(command, check=True)
        result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
        replay = Path(tmp) / 'replay'
        subprocess.run(command[:5] + [*fp.test_include_flags(), str(HERE / 'test_world_moving_replay.cpp'), str(fp.src('world_gi.cpp')), '-o', str(replay)], check=True)
        result.stdout += subprocess.run([str(replay)], check=True, capture_output=True, text=True).stdout
    report = {
        'result': 'passed', 'runtime_game_test': False,
        'reproduce': 'python3 tests/validate_world_dynamic_probes.py',
        'sanitizers': ['address', 'undefined'], 'output': result.stdout.splitlines(),
        'limits': {
            'dynamic_probes_per_publication': 512,
            'dynamic_solves_per_publication': 128,
            'correction_reuse': 'Valid while the anchored (0.5-unit hysteresis, one-unit quantized) bounds of actors within 48 units are unchanged; budget spent nearest tier 32 then oldest first; a mismatched correction is applied at most 8 captures; unused entries retained 16 captures.',
            'full_dynamic_influence_distance_from_draw_bounds': 12,
            'zero_dynamic_influence_distance_from_draw_bounds': 24,
            'actor_capture': 'Visible submitted geometry only; offscreen actors are not inferred.',
            'dynamic_path_replay': 'Static-only ray paths recorded once per probe and static generation (4 MiB budget); a re-solve replays their moving queries any-hit and retraces only touched rays. Bit-identical to the direct moving solve.',
            'dynamic_visibility': 'Static validity and distance moments retained; moving geometry changes radiance only.',
            'static_generation': 'Map/geometry/lighting changes still invalidate the static cache.'
        },
        'sha256': {name: hashlib.sha256(fp.tracked(name).read_bytes()).hexdigest() for name in sources}
    }
    (fp.output_dir() / 'world-dynamic-probes-validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(result.stdout, end='')


if __name__ == '__main__':
    main()
