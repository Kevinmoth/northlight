#!/usr/bin/env python3
"""Identify a client's archive chain and match it to a prebuilt world-cache variant (stdlib only, no StormLib).

    python3 scripts/client_identity.py --client C [--locale enUS] [--variants DIR_OR_JSON ...]

The chain is client_archives.chain(client, 'all', locale, without): what the game loads. without
defaults to 'z', our own art layer. The caller passes without='' when its patch-z ownership check
(D11) finds that the z archives are not ours: a foreign z is client content, and then shows up as a
difference. Each archive gets an identity {role, name, bytes, table_sha256}:
- role 'base' (Data/) or 'locale' (Data/<locale>/); the name is lower case with the locale folded
  to <loc> (patch-<loc>-3.mpq), so an identity does not depend on the locale;
- table_sha256 is the sha256 of the MPQ header and its raw hash, block and hi-block tables (plus
  the HET and BET tables from format 3 on), read at the header's offsets, after an MPQ\\x1b
  user-data header if there is one. Nothing is decrypted; it takes milliseconds per archive and
  changes whenever a file is added, removed or resized;
- Blizzard's localized archives (locale-, expansion-locale-, lichking-locale-<loc>.mpq and
  patch-<loc>.mpq, -2, -3) have only role and name: they hold no world or Light data, and their
  bytes differ per locale;
- an archive that cannot be read as an MPQ (truncated, not an MPQ, no permission) gets
  {role, name, unreadable: reason} and never matches: such a client identifies as no variant.

A variant manifest (northlight-cache.json, format northlight-cache-variant/1, see build_cache_variant.py)
holds the identity of the chain its cache was built from. A client matches a variant when its
chain has the same archives in the same order with the same identities; any extra, missing or
changed archive is a difference, because without StormLib nothing can tell whether an unknown
archive carries world data.
"""
import argparse
import hashlib
import json
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import client_archives  # noqa: E402

FORMAT = 'northlight-client-identity/1'
ART_LETTER = 'z'
BLIZZARD_LOCAL = {'locale-<loc>.mpq', 'expansion-locale-<loc>.mpq', 'lichking-locale-<loc>.mpq',
                  'patch-<loc>.mpq', 'patch-<loc>-2.mpq', 'patch-<loc>-3.mpq'}
# The installer package keeps its variant manifests in <package>/variants/ (this file is
# <package>/app/scripts/client_identity.py).
VARIANTS = Path(__file__).resolve().parents[2] / 'variants'
MAX_TABLE = 256 << 20


def table_sha256(path):
    """sha256 of an MPQ's header (and user-data header) and its raw file tables."""
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        size = os.fstat(f.fileno()).st_size
        base, magic = 0, f.read(4)
        if magic == b'MPQ\x1b':
            f.seek(0)
            user = f.read(16)
            h.update(user)
            base = struct.unpack_from('<I', user, 8)[0]
            f.seek(base)
            magic = f.read(4)
        if magic != b'MPQ\x1a':
            raise ValueError(f'{path}: not an MPQ archive')
        f.seek(base + 4)
        header_size = struct.unpack('<I', f.read(4))[0]
        f.seek(base)
        header = f.read(min(max(header_size, 32), 208))
        if len(header) < 32:
            raise ValueError(f'{path}: truncated MPQ header')
        h.update(header)
        version, hash_pos, block_pos, hash_count, block_count = struct.unpack_from('<H2x4I', header, 12)
        hi_block = hash_hi = block_hi = het = bet = 0
        if version >= 1 and len(header) >= 44:
            hi_block, hash_hi, block_hi = struct.unpack_from('<QHH', header, 32)
        if version >= 2 and len(header) >= 68:
            bet, het = struct.unpack_from('<QQ', header, 52)
        sizes = struct.unpack_from('<5Q', header, 68) if version >= 3 and len(header) >= 108 else None
        tables = [((hash_hi << 32) | hash_pos, sizes[0] if sizes else hash_count * 16),
                  ((block_hi << 32) | block_pos, sizes[1] if sizes else block_count * 16)]
        if hi_block:
            tables.append((hi_block, sizes[2] if sizes else block_count * 2))
        for pos, known in ((het, sizes[3] if sizes else 0), (bet, sizes[4] if sizes else 0)):
            if pos and not known:   # format 3: the extended table header <sig, version, data size>
                f.seek(base + pos)
                ext = f.read(12)
                known = 12 + struct.unpack_from('<I', ext, 8)[0] if len(ext) == 12 else 0
            if pos:
                tables.append((pos, known))
        for pos, length in tables:
            length = max(0, min(length, MAX_TABLE, size - base - pos))
            f.seek(base + pos)
            data = f.read(length)
            h.update(struct.pack('<QQ', pos, len(data)))
            h.update(data)
    return h.hexdigest()


def archive_identity(path, client, locale):
    rel = Path(path).relative_to(client)
    role = 'locale' if len(rel.parts) == 3 else 'base'
    name = rel.name.lower()
    if role == 'locale':
        name = name.replace(locale.lower(), '<loc>')
    entry = {'role': role, 'name': name}
    if not (role == 'locale' and name in BLIZZARD_LOCAL):
        try:
            entry.update(bytes=Path(path).stat().st_size, table_sha256=table_sha256(path))
        except (OSError, ValueError, struct.error) as e:
            return {'role': role, 'name': name, 'unreadable': str(e).replace(str(client), '<client>')}
    return entry


def chain_identity(client, archives='all', locale=None, without=ART_LETTER):
    """{'format', 'locale', 'chain': [archive identities], 'sha256'} of a client's chain."""
    client = Path(client)
    locale = client_archives.detect_locale(client, locale)
    chain = [archive_identity(p, client, locale) for p in client_archives.chain(client, archives, locale, without)]
    return {'format': FORMAT, 'locale': locale, 'chain': chain, 'sha256': digest(chain)}


def digest(chain):
    return hashlib.sha256(json.dumps(chain, sort_keys=True).encode()).hexdigest()


def label(entry):
    return ('Data/<loc>/' if entry['role'] == 'locale' else 'Data/') + entry['name']


def match(identity, variant):
    """(ok, differences) of a chain identity (or its 'chain' list) against a variant manifest (or its
    'identity')."""
    have = identity['chain'] if isinstance(identity, dict) else identity
    want = (variant.get('identity') or variant)['chain']
    key = lambda e: (e['role'], e['name'])  # noqa: E731
    have_keys, wanted = [key(e) for e in have], {key(e): e for e in want}
    differences = [f'missing {label(e)}' for e in want if key(e) not in have_keys]
    differences += [f'extra {label(e)}' for e in have if key(e) not in wanted]
    differences += [f"unreadable {label(e)} ({e['unreadable']})" for e in have if 'unreadable' in e]
    for e in have:
        w = wanted.get(key(e))
        if w is not None and 'table_sha256' in w and 'unreadable' not in e and \
                (e.get('bytes'), e.get('table_sha256')) != (w['bytes'], w['table_sha256']):
            differences.append(f"changed {label(e)} ({e.get('bytes')} bytes; the variant has {w['bytes']} bytes"
                               + (', other tables)' if e.get('bytes') == w['bytes'] else ')'))
    if not differences and have_keys != [key(e) for e in want]:
        differences.append('the archive order differs')
    return not differences, differences


def load_variants(source=None):
    """{name: manifest} from a folder (*.json, */northlight-cache.json), a file, a list or a dict of manifests."""
    source = VARIANTS if source is None else source
    if isinstance(source, dict):
        return {source.get('variant', 'variant'): source} if 'identity' in source else dict(source)
    if isinstance(source, (list, tuple)):
        found = {}
        for item in source:
            found.update(load_variants(item))
        return found
    path = Path(source)
    if path.is_dir():
        files = sorted(path.glob('*.json')) + sorted(path.glob('*/northlight-cache.json'))
    else:
        files = [path] if path.is_file() else []
    found = {}
    for f in files:
        data = json.loads(f.read_text(encoding='utf-8'))
        if isinstance(data, dict) and data.get('identity') and data.get('variant'):
            found.setdefault(data['variant'], data)
    return found


def identify(client, locale=None, variants=None, without=ART_LETTER):
    """{'locale', 'chain', 'sha256', 'variant': name or None, 'differences': [...]} of a client.
    variants: see load_variants (default: the package's variants/ folder). Without a match,
    'closest' names the variant with the fewest differences, and 'differences' lists them.
    without: 'z' (default) when the client's patch-z/patch-<loc>-z are ours or absent; '' when the
    caller's ownership check says they are foreign, so they count as client archives.
    An archive that cannot be read is a difference ('unreadable ...'): no variant matches."""
    identity = chain_identity(client, 'all', locale, without)
    known = load_variants(variants)
    result = {'locale': identity['locale'], 'chain': identity['chain'], 'sha256': identity['sha256'],
              'variant': None, 'closest': None, 'differences': ['no variant manifests found'] if not known else []}
    best = None
    for name, manifest in sorted(known.items()):
        ok, differences = match(identity, manifest)
        if ok:
            result.update(variant=name, closest=name, differences=[])
            return result
        if best is None or len(differences) < len(best[1]):
            best = (name, differences)
    if best:
        result.update(closest=best[0], differences=best[1])
    return result


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--client', type=Path, required=True)
    ap.add_argument('--locale')
    ap.add_argument('--variants', nargs='+', type=Path, help=f'variant manifests or folders (default: {VARIANTS})')
    ap.add_argument('--without', default=ART_LETTER, help="our art letter to leave out (default z; '' when z is foreign)")
    args = ap.parse_args(argv)
    result = identify(args.client, args.locale, args.variants, args.without)
    print(json.dumps(result, indent=2))
    return 0 if result['variant'] else 1


if __name__ == '__main__':
    sys.exit(main())
