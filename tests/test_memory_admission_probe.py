#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Compile the actual admission helper against changing native memory maps."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
OUT = fp.output_dir()
OUT.mkdir(exist_ok=True)
report = {
    'scope': 'Actual production memory admission address-witness helper; simulated fresh Win32 status/region calls; no game, Wine or graphics execution.',
    'virtual_query_contract': 'https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualquery',
    'source_sha256': {name: hashlib.sha256(fp.tracked(name).read_bytes()).hexdigest() for name in ['memory_admission_probe.h', 'geometry_memory.h', 'test_memory_admission_probe.cpp', Path(__file__).name]},
    'runs': [],
}
with tempfile.TemporaryDirectory(prefix='memory-admission-probe-') as temp:
    for mode, flags in [('O2', ['-O2']), ('ASan-UBSan', ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'])]:
        binary = Path(temp) / mode
        command = ['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', *flags, *fp.test_include_flags(), str(HERE / 'test_memory_admission_probe.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True)
        result = subprocess.run([str(binary)], text=True, capture_output=True)
        (OUT / ('memory-admission-probe-' + mode + '.txt')).write_text(result.stdout + result.stderr)
        print(result.stdout + result.stderr, end='')
        result.check_returncode()
        report['runs'].append({'mode': mode, 'compile_command': command, 'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
(OUT / 'memory-admission-probe-validation.json').write_text(json.dumps(report, indent=2) + '\n')
