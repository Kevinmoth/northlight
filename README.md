# Northlight renderer

A Direct3D 9 extension for the World of Warcraft 3.3.5a client: a proxy `d3d9.dll`
(built as `frd9.dll`) that adds lighting, shadows, fog, GI and sky effects. The repository also
holds the offline asset builders (MPQ patches, world cache) and their validation.

The repository holds no game files: every tool that needs game data reads it from your own
3.3.5a client. Licensed under the [MIT License](LICENSE).

The changelog, release notes and validation records are not part of the public source.

## Layout

| Path | Contents |
|---|---|
| `src/<group>/` | DLL sources, grouped: `proxy` (the D3D9 proxy and device mirror), `core` (settings, memory, logging, profiling), `world`, `replay`, `shadows`, `gi`, `lights`, `sky`, `water`, `gamedata` (tables derived from the game's MPQs), `generated` (headers written by the generators and shader compilers; do not edit) |
| `shaders/` | `*.hlsl` and their `*-shader-build.json` manifests; `shaders/compiled/` holds the compiled `<Entry>.bin` and `.bin.asm` |
| `scripts/` | `build_renderer.py`, `build_environment.py`, `generate_*.py`, `run_tests.py`, `check_layout.py`, `pe_normalized_hash.py`; `scripts/shaders/` holds the shader compilers (`compile_*_shaders.py`, `compile_shaders.cpp`, `disassemble_*.cpp`) |
| `renderer/` | `windows-package/`, `mac-package/`, the player installer `northlight_install.py`, the pipeline and packaging tools (`world_*_builder.py`, `extract_*.py`, `migrate_mac_proxy.py`, `build_packages.py`, `package-pins.json`, ...) and the built `frd9.dll` |
| `tests/` | test runners (`test_*.py`), native tests (`test_*.cpp/.h`), release validators (`validate_*.py`, `verify_*.py`) |
| `tests/support/`, `tests/fixtures/` | d3d9.h/windows.h shims for native builds; the archived baselines (old sources of this project, digests) that tests compare against. Real client data (shaders, doodad placements) is read from your client by `tests/client_fixtures.py`, never stored |
| `northlight_paths.py` | the only place that knows where the client, toolchain and outputs are |
| `client-config/` | tracked copies of the client-root profile `.ini` files |
| `*.py` (top level) | the art layer (MPQ patch) builder `build_art_layer.py` and its steps, `mpq.py`, `client_archives.py`, `renderer_status.py` (install/uninstall on macOS) |
| `out/` | build and test output (ignored) |

`tools/` and `backups/` are local and untracked.

## Requirements

- macOS on Apple Silicon is the supported development host. Linux may work with zig, clang and
  Wine on `PATH`, but it is untested. Windows is a target only: the DLL runs there, but the
  development scripts do not.
- Python 3.9 or newer; only the standard library is used.
- [zig 0.15.2](https://ziglang.org/download/) to build the DLL (it cross-compiles to 32-bit Windows).
- clang++ and c++ (Xcode command line tools) for the native tests.
- Optional:
  - a WoW 3.3.5a client (game data, world cache);
  - Wine, for the shader compilers only (`scripts/shaders/compile_*_shaders.py`);
  - StormLib, for reading MPQs;
  - the external archive and `backups/`, for differential tests against old versions.

## Build

```sh
python3 scripts/build_renderer.py           # -> renderer/frd9.dll
python3 scripts/pe_normalized_hash.py renderer/frd9.dll
```

The build first runs the layout check (nothing back at a pre-move path, unique basenames). It
regenerates the forwarding headers in `src/generated/`. It checks every `shaders/*.hlsl` against its
`*-shader-build.json` and refuses to build if a shader changed without a recompile. The raw
DLL bytes depend on the build directory, because the PDB paths feed the build ID. Compare builds
with `pe_normalized_hash.py`, which zeroes only those fields.

After editing a shader, recompile it (Wine, compiler only; nothing touches the game):

```sh
python3 scripts/shaders/compile_world_shaders.py   # or compile_shaders, compile_water_shaders, ...
```

A compiler writes `shaders/compiled/<Entry>.bin` and `.bin.asm`, `src/generated/*_compiled_shaders.h`
and the `shaders/*-shader-build.json` manifest. `build_environment.py` keeps the Wine prefix
outside the client folder.

## Test

```sh
python3 scripts/run_tests.py                     # all tests/test_*.py, 4 at a time, nice 15
python3 scripts/run_tests.py test_replay_*       # by pattern; validate_*/verify_* only when named
python3 scripts/run_tests.py --require client    # a missing client FAILs instead of SKIPping
python3 scripts/run_tests.py --record 0.3.158    # write reports to renderer/records/validation-0.3.158
```

Reports go to `out/test-output/<run>/<test>/`. Nothing is written into the tree unless you
pass `--record` (`renderer/records/` is ignored by git, and no test reads from it). A test
declares what it needs on one line under the shebang:

```python
# northlight-test: requires=cxx,client timing
```

| Tag | Meaning |
|---|---|
| `cxx` | host `clang++`/`c++` |
| `client` | a WoW client (see `NORTHLIGHT_CLIENT`) |
| `zig`, `wine`, `stormlib` | toolchain pieces (see below) |
| `stormlib-src` | the pinned StormLib source tree in `tools/StormLib-master` (`scripts/build_stormlib.py`) |
| `world-cache` | the client's world cache, `<client>/world-cache` (`scripts/install_world_cache.py`) |
| `dll` | a built `renderer/frd9.dll` (build first) |
| `backups`, `archive` | old renderer tarballs / the external archive, for differential tests |
| `base` | `BASE=<pristine older renderer tree>` for a release gate |
| `timing` | CPU-time budgets: the test runs alone after the others |
| `slow` | started first |
| `manual` | needs arguments; runs only when named |
| `known-fail=<reason>` | last on the line; fails at the current version for a known reason ([tests/KNOWN_FAILURES.md](tests/KNOWN_FAILURES.md)): still runs, reported XFAIL (XPASS if it passes), does not fail the run |

A test whose requirement is missing is reported as SKIP with the reason. `tests/discovery.txt`
lists the tests the runner must find. After adding a test, run with `--update-discovery`.

With no client, toolchain or archive you can still build the DLL (given zig) and run most
tests, which are pure Python or native C++. Tests that run on real client data read it from your
client through `tests/client_fixtures.py`: the four-bone skin and the one-influence shader programs
from the stock archives (`client,stormlib`), and real doodad placements from the world cache
(`world-cache`).

About 50 `tests/test_*.cpp` files have no Python runner: no runner executes them, and they are
built by hand as their docs describe (for example
[tests/support/gpu_profile/README.md](tests/support/gpu_profile/README.md)). The release tool
`tests/verify_packages.py` checks the zips `renderer/build_packages.py` wrote (run it by name).

## Machine configuration

Nothing in the code names a machine path. Each setting comes from, in order, an environment
variable, then `northlight.local.ini` (copy [northlight.local.ini.example](northlight.local.ini.example);
it is ignored by git), then a default. `python3 northlight_paths.py` prints what resolves.

| Variable | Default | Used for |
|---|---|---|
| `NORTHLIGHT_CLIENT` | the repository's parent folder, if it holds `Wow.exe` and `Data/` | client data for tests, validators and pipeline scripts (not `renderer_status.py`, see below) |
| `NORTHLIGHT_ZIG` | `tools/zig-*/zig`, then `zig` on `PATH` (must be 0.15.2) | DLL build, generators, shader tools |
| `NORTHLIGHT_WINE` | WoWSilicon's bundled Wine, then `wine` on `PATH` | shader compilers only |
| `NORTHLIGHT_STORMLIB` | `tools/storm-build/storm.framework/storm` | `mpq.py` |
| `NORTHLIGHT_TOOLS` | `tools/` | third-party downloads (DXVK, StormLib, zig) |
| `NORTHLIGHT_ARCHIVE` | none | old rollbacks and runtime diagnostics (`<archive>/renderer/`) |
| `NORTHLIGHT_BACKUPS` | `backups/` | old renderer tarballs |
| `NORTHLIGHT_OUT` | `out/` | build and test output |
| `NORTHLIGHT_TEST_OUTPUT_DIR` | set by `run_tests.py` | where a test writes its report |
| `NORTHLIGHT_SHADER_CORPUS` | none | optional captured shader corpus (extra sub-cases) |
| `NORTHLIGHT_REFERENCE_EXE` | none | optional second client's `wow.exe`, compared by `renderer_status.py status` |
| `NORTHLIGHT_DOWNLOADS` | none (then `tools/`) | pinned runtime downloads for `renderer/build_packages.py` (`renderer/package-pins.json`) |
| `NORTHLIGHT_LIVE_CLIENT`, `NORTHLIGHT_STOCK_CLIENT`, `NORTHLIGHT_LIGHTS_STAGE`, `BASE` | none | a few specific tests; see their docstrings (`NORTHLIGHT_STOCK_CLIENT`: a stock 3.3.5a client for the identity and variant tests) |

## Conventions

- **Basenames are the include namespace.** C++ includes use bare names (`#include "world_gi.h"`),
  each `src/<group>/` is one `-I` (`northlight_paths.include_flags()`), and Python finds sources the
  same way, with `northlight_paths.src('world_gi.h')` (sources and shaders) or `tracked()` (also
  scripts and tests). Directories only group files: moving a file between groups needs no edit.
  Every basename must be unique; `scripts/check_layout.py` enforces this.
- A test starts with the tag line and these two lines, then uses `fp.src(...)`,
  `fp.test_include_flags()`, `fp.output_dir()` and so on:
  ```python
  import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
  import northlight_paths as fp
  ```
- Tests never write next to the sources. They write to `fp.output_dir()`.
- `.gitattributes` has `* -text`: hash gates compare raw bytes, so keep LF line endings.
- The pre-commit hook rejects files over 5 MB and game or binary payloads, and it runs
  `check_layout.py`. Enable it in each clone with `git config core.hooksPath githooks`.
- `git clean -x` also deletes the ignored local folders (`tools/`, `backups/`, `out/`).

## Install on macOS (WoWSilicon)

With the game closed, from the client folder:

```sh
python3 <repository>/renderer_status.py status
python3 <repository>/renderer_status.py on      # or: off
```

`renderer_status.py` always treats the repository's parent folder as the client and ignores
`NORTHLIGHT_CLIENT`, so install and uninstall work only when the repository sits inside the client folder.
`on` installs `renderer/frd9.dll` as `mods/d3d9.dll`, and `off` restores the recorded
transaction. Add `--dry-run` to preview. The Windows package template is in
`renderer/windows-package/`.
