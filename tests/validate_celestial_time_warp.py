#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native deterministic test of the renderer-owned native-speed sun/moon orbit and its two direct
consumers' tests (twilight fill, static shadow request prewarm); never starts a game."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def main():
    # 0.3.177: the twilight fill and static-request tests follow the orbit and run here too.
    tests = ['test_celestial_time_warp.cpp', 'test_twilight_fill.cpp', 'test_static_shadow_request.cpp']
    sources = ['celestial_time_warp.h', 'twilight_fill.h', 'static_shadow_request.h', *tests]
    output = []
    with tempfile.TemporaryDirectory(prefix='northlight-celestial-warp-') as tmp:
        for test in tests:
            binary = Path(tmp) / Path(test).stem
            subprocess.run(['clang++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
                            *fp.test_include_flags(), str(HERE / test), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            if result.returncode:
                print(test, result.stdout, result.stderr)
                result.check_returncode()
            output += result.stdout.splitlines()
    report = {
        'result': 'passed', 'runtime_game_test': False,
        'reproduce': 'python3 tests/validate_celestial_time_warp.py',
        'sanitizers': ['address', 'undefined'], 'output': output,
        'policy': {
            'clock': 'validated client render day fraction (map fixed-time overrides included)',
            'solar_policy': 'native five-key piecewise-linear polar table; no accelerated phases',
            'sunrise': '06:10:31.579', 'sun_crest': '11:55-12:05 (~85 degrees)', 'sunset': '20:30:31.578',
            'moonrise': 'solar sunset', 'moonset': 'solar sunrise', 'moon_crest': '00:00 (43 degrees)',
            'lunar_policy': 'four quarter-sines, day-side hold, crest 00:00 (43), horizon 19.4/10.9 deg/h',
            'azimuth_degrees': 45, 'history_or_uptime_dependency': False,
            'fail_safe': 'invalid clock returns invalid result; context left unchanged',
        },
        'sha256': {name: hashlib.sha256(fp.tracked(name).read_bytes()).hexdigest() for name in sources},
    }
    (fp.output_dir() / 'celestial-time-warp-validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print('\n'.join(output))


if __name__ == '__main__':
    main()
