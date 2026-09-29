#!/usr/bin/env python3
# northlight-test: requires=client,stormlib
"""build_art_layer on the configured client (reads its MPQs and world-cache/fog; writes only to
the test output):
- stock view: the relighting plus the Mulgore and Stormwind bands. Mulgore (Light 201/202/234)
  changes colour bands 1-7 and 12 and fog bands 0-1 of its profile; Stormwind (Light 51/52/77)
  the fog end of its profiles. Both record their sky clone as skipped, every other HD sky step is
  skipped with a reason (not a KeyError), and the archive holds the four band tables, no sky clone
  and no moon02. Against the relighting plus the retime, exactly those band rows differ.
- both views (stock and the client's own chain), against the relighting alone: no band whose key
  times lie within {00:00, 12:00} changes, every changed band belongs to a profile the relighting
  created, every band holds <= 16 keys after all steps, and every band the retime moved without a
  skipped insert is night-like at 21:30 and 03:30.
- the client's own chain: when the client has our art layer installed (Data/patch-z.mpq), the
  rebuild without it is byte-identical to it, so the installer step reproduces the HD chain, and
  the Mulgore and Stormwind steps run with their sky clones."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
import hashlib, json, shutil
import build_art_layer, build_lighting, client_archives
from build_lighting import DBC, u
from mpq import Archive
from world_scene_builder import Assets

client = fp.client_root()
out = fp.output_dir()
cache = client / 'world-cache'
tables = {f'DBFilesClient\\{n}.dbc' for n in ('Light', 'LightParams', 'LightIntBand', 'LightFloatBand')}
report = {}

stock = out / 'stock'
shutil.rmtree(stock, ignore_errors=True)
r = build_art_layer.build(client, stock, cache, archives='stock')
skipped = {s['step']: s['skipped'] for s in r['steps'] if 'skipped' in s}
assert set(skipped) == {'orgrimmar', 'outdoor_sun', 'outdoor_moon'}, r['steps']
sky_skipped = {s['step']: s['sky_clone_skipped'] for s in r['steps'] if 'sky_clone_skipped' in s}
assert set(sky_skipped) == {'mulgore', 'stormwind'}, r['steps']
assert set(r['files']) == tables, sorted(r['files'])
assert r['steps'][0]['profiles'] > 0 and not r['steps'][0]['skipped_incomplete_profiles']
for target in r['targets']:
    with Archive(stock / target) as a:
        assert {n for n in a.names() if not n.startswith('(')} == tables
# The relighting alone, from the same stock view, against the archive: only the two zones' bands.
assets = Assets(client, 'stock', r['locale'], without='z')
try:
    relit = {n: DBC(assets.read(f'DBFilesClient\\{n}.dbc')) for n in build_lighting.TABLES}
finally:
    assets.close()
changes, _, _ = build_lighting.relight(relit)
build_lighting.retime(relit, [c['new'] for c in changes.values()])
with Archive(stock / r['targets'][1]) as a:
    built = {n: DBC(a.read(f'DBFilesClient\\{n}.dbc')) for n in build_lighting.TABLES}
mulgore = {u(relit['Light'].index[i], 7) for i in (201, 202, 234)}
stormwind = {u(relit['Light'].index[i], 7) for i in (51, 52, 77)}
assert len(mulgore) == 1 and len(stormwind) == 2 and not mulgore & stormwind, (mulgore, stormwind)
(m,) = mulgore
want = {'Light': set(), 'LightParams': set(),
        'LightIntBand': {(m-1)*18+c+1 for c in (1, 2, 3, 4, 5, 6, 7, 12)},
        'LightFloatBand': {(m-1)*6+1, (m-1)*6+2} | {(p-1)*6+1 for p in stormwind}}
for n, table in relit.items():
    assert set(table.index) == set(built[n].index) and table.strings == built[n].strings, n
    changed = {i for i, row in table.index.items() if built[n].index[i] != row}
    assert changed == want[n], (n, sorted(changed), sorted(want[n]))
report['stock'] = {'profiles': r['steps'][0]['profiles'], 'skipped': skipped, 'sky_clone_skipped': sky_skipped,
                   'mulgore_profile': m, 'stormwind_profiles': sorted(stormwind),
                   'changed_band_rows': {n: sorted(v) for n, v in want.items() if v}, 'files': sorted(r['files'])}


def night_like(pairs, floating, time):
    night = build_lighting.band_value(pairs, floating, 0)
    spread = max(build_lighting.band_difference(v if floating else build_lighting.rgb(v), night, floating) for _, v in pairs)
    tolerance = max(1e-3, .1*spread) if floating else max(3, .1*spread)
    return build_lighting.band_difference(build_lighting.band_value(pairs, floating, time), night, floating) <= tolerance


def real_tables(view, folder, r):
    """The retime's scope on the client's real tables, for one archive view."""
    assets = Assets(client, view, r['locale'], without='z')
    try:
        relit = {n: DBC(assets.read(f'DBFilesClient\\{n}.dbc')) for n in build_lighting.TABLES}
    finally:
        assets.close()
    changes, _, _ = build_lighting.relight(relit)
    created = {c['new'] for c in changes.values()}
    with Archive(folder / r['targets'][1]) as a:
        built = {n: DBC(a.read(f'DBFilesClient\\{n}.dbc')) for n in build_lighting.TABLES}
    retime = next(s for s in r['steps'] if s['step'] == 'retime')
    skipped_insert = {f"{s['table']}:{s['id']}" for s in retime['insert_skipped']}
    result = {'retimed': retime['retimed'], 'unchanged': retime['unchanged'], 'inserted_keys': retime['inserted_keys'],
              'insert_skipped': len(skipped_insert)}
    for name, floating, channels in (('LightIntBand', False, 18), ('LightFloatBand', True, 6)):
        for id, row in built[name].index.items():
            assert 0 <= u(row, 1) <= 16, (name, id)
            before = relit[name].index.get(id)
            if before is None or before == row:
                continue
            times = {u(before, 2+i) for i in range(u(before, 1))}
            assert not times <= {0, 1440}, (name, id, sorted(times))   # two-key zones (Tirisfal) unchanged
            assert (id-1)//channels+1 in created, (name, id)         # only the relighting's private profiles
        for key in r['retimed_rows']:
            table, id = key.split(':')
            if table != name or key in skipped_insert:
                continue
            pairs = build_lighting.band_pairs(built[name].index[int(id)], floating)
            assert night_like(pairs, floating, 2580) and night_like(pairs, floating, 420), (key, pairs)
    return result


report['stock']['retime'] = real_tables('stock', stock, r)
installed = client_archives.data_dir(client) / 'patch-z.mpq'
own = out / 'own'
shutil.rmtree(own, ignore_errors=True)
r_own = build_art_layer.build(client, own, cache)
report['own_retime'] = real_tables('all', own, r_own)
if installed.is_file():
    r = r_own
    want = hashlib.sha256(installed.read_bytes()).hexdigest()
    got = [hashlib.sha256((own / t).read_bytes()).hexdigest() for t in r['targets']]
    report['own'] = {'installed_sha256': want, 'rebuilt_sha256': got, 'steps': [s['step'] for s in r['steps'] if 'skipped' not in s]}
    assert got == [want, want], report['own']
    assert not any('skipped' in s or 'sky_clone_skipped' in s for s in r['steps']), r['steps']
(out / 'art-layer.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
