#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native deterministic test of the renderer-owned native-speed sun/moon orbit; never starts a game."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def main():
    sources = ['celestial_time_warp.h', 'test_celestial_time_warp.cpp']
    with tempfile.TemporaryDirectory(prefix='northlight-celestial-warp-') as tmp:
        binary = Path(tmp) / 'test'
        subprocess.run(['clang++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
                        *fp.test_include_flags(), str(HERE / 'test_celestial_time_warp.cpp'), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        if result.returncode:
            print(result.stdout, result.stderr)
            result.check_returncode()
    report = {
        'result': 'passed', 'runtime_game_test': False,
        'reproduce': 'python3 tests/validate_celestial_time_warp.py',
        'sanitizers': ['address', 'undefined'], 'output': result.stdout.splitlines(),
        'policy': {
            'clock': 'validated client render day fraction (map fixed-time overrides included)',
            'solar_policy': 'native five-key piecewise-linear polar table; no accelerated phases',
            'sunrise': '06:10:31.579', 'sun_crest': '11:55-12:05 (~85 degrees)', 'sunset': '20:30:31.578',
            'moonrise': 'solar sunset', 'moonset': 'solar sunrise', 'moon_crest': '00:00 (43 degrees)',
            'lunar_policy': 'renderer-owned full-length C1 cosine segments, one stable nocturnal arc',
            'azimuth_degrees': 45, 'history_or_uptime_dependency': False,
            'fail_safe': 'invalid clock returns invalid result; context left unchanged',
        },
        'sha256': {name: hashlib.sha256(fp.tracked(name).read_bytes()).hexdigest() for name in sources},
    }
    (fp.output_dir() / 'celestial-time-warp-validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(result.stdout, end='')


if __name__ == '__main__':
    main()
