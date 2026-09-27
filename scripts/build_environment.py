"""Keep Wine's host-root symlinks OUTSIDE WoW's recursively indexed directory; toolchain lookup."""
from pathlib import Path
import hashlib
import os
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import northlight_paths  # noqa: E402

# The prefix must stay outside the client, and outside this repository when it sits in one.
CLIENT_ROOT = northlight_paths.client_root(required=False) or northlight_paths.REPO.parent
TOOLS = northlight_paths.tools()


def __getattr__(name):
    # Resolved on first use, so importers that never touch them need no archive, Wine or zig.
    # ARCHIVE: rollbacks, runtime diagnostics and old backups live outside the client folder
    # (NORTHLIGHT_ARCHIVE, the folder holding renderer/).
    if name == 'ARCHIVE':
        return northlight_paths.archive()
    if name == 'WINE_ROOT':
        return northlight_paths.wine_root()
    if name == 'ZIG':
        return northlight_paths.zig()
    raise AttributeError(name)


def zig_env():
    return northlight_paths.zig_env()


def wine_build_prefix():
    suffix = hashlib.sha256(str(CLIENT_ROOT).encode()).hexdigest()[:12]
    parent = Path(tempfile.gettempdir()) / f'northlight-renderer-{os.getuid()}-{suffix}'
    prefix = (parent / 'wine-prefix').resolve()
    if prefix == CLIENT_ROOT or CLIENT_ROOT in prefix.parents:
        raise RuntimeError('Wine build prefix must be outside the client directory.')
    parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    return prefix
