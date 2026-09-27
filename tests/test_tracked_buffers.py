#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Build the production tracked-buffer template against native fake COM types."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parent
out = fp.output_dir()
stubs = out / "tracked-buffer-stubs"
stubs.mkdir(parents=True, exist_ok=True)
(stubs / "d3d9.h").write_text("#pragma once\n// Native fake D3D types are declared by test_tracked_buffers.cpp.\n")
report = {"scope": "Production tracked_buffers.h template and identity/batch functions, native fake COM resources. No D3D runtime, Wine or game launch.",
          "source_sha256": {str(fp.tracked(name)): hashlib.sha256(fp.tracked(name).read_bytes()).hexdigest() for name in ["tracked_buffers.h", "capture_buffer_metadata.h", "test_tracked_buffers.cpp", "test_tracked_buffers.py"]},
          "runs": [],
          "limitations": ["Fake COM and descriptors exercise the contract, not driver behavior.", "Concurrent first identity assignment is tested; sanitizers are ASan/UBSan, not a race detector.", "The one registry-lock-per-batch property is verified by source inspection; the test confirms no descriptor/private-data calls occur during batch reads."]}
for suffix, flags in [("", ["-O2"]), ("_san", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])]:
    binary = out / ("test_tracked_buffers" + suffix)
    commands = [["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread", *flags, "-I" + str(stubs), *fp.test_include_flags(), str(root / "test_tracked_buffers.cpp"), "-o", str(binary)], [str(binary)]]
    entry = {}
    for label, command in zip(["compile", "run"], commands):
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        entry[label] = {"command": shlex.join(command), "exit_code": result.returncode, "stdout": result.stdout, "stderr": result.stderr}
        if result.returncode:
            report["runs"].append(entry)
            (out / "tracked-buffer-validation.json").write_text(json.dumps(report, indent=2) + "\n")
            raise RuntimeError(f"{label} failed: {result.stdout}{result.stderr}")
    report["runs"].append(entry)
    print(entry["run"]["stdout"], end="")
(out / "tracked-buffer-validation.json").write_text(json.dumps(report, indent=2) + "\n")
print(out / "tracked-buffer-validation.json")
