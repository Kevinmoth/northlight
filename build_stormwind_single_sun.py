"""Clone Stormwind's HD sky without its extra sun; slightly denser day fog.

Input must be the pre-change lighting MPQ. Never launch the game.
Two parts: (a) Stormwind's denser day fog on the profiles of its Light rows;
(b) the clone of the city's HD sky without its sun. (b) is skipped with the
reason when the profiles are not the HD ones, and (a) still applies. Orgrimmar
has only (b), so the whole step is skipped then.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
from mpq import Archive
from build_lighting import DBC, u, f, putu, require, Skip
from build_mulgore_lighting import rewrite, strength, sample_float

CLIENT = Path(__file__).resolve().parent.parent
LIGHT_IDS = {51, 52, 77, 2543, 2554, 2567}
# Rows the HD pack added around the city; stock Stormwind is 51, 52 and 77 alone.
HD_LIGHT_IDS = {2543, 2554, 2567}
PROFILES = {958, 1133, 1134, 1136}
CUSTOM = 'Environments\\Stars\\fr_stormwind_single_sun'
OMIT = {'stormwind_sun.blp', 'stormwind_sun_sunset.blp'}


def build(source, output, city='stormwind', assets=None, fog=None):
    """assets: a world_scene_builder.Assets of the client (default: the configured client); fog:
    the world cache's fog folder (default: <client>/world-cache/fog). Raises build_lighting.Skip
    when the input lacks the city's Light rows, or (Orgrimmar) its HD profiles or sky."""
    if city == 'stormwind':
        light_ids, hd_rows, hd_profiles, custom, omit = LIGHT_IDS, HD_LIGHT_IDS, PROFILES, CUSTOM, OMIT
        map_name, map_id, zone, sky_id, batches, removed_count = 'Azeroth', 0, 1519, 170, 29, 2
    else:
        light_ids, hd_rows, hd_profiles = {239}, set(), {1003}
        custom = 'Environments\\Stars\\fr_orgrimmar_single_sun'
        omit = {'orgrisunray01.blp', 'orgrisunflare01.blp', 'orgrilensflare02.blp'}
        map_name, map_id, zone, sky_id, batches, removed_count = 'Kalimdor', 1, 1637, 165, 38, 3
    with Archive(source) as a:
        original = {n: a.read(n) for n in a.names() if not n.startswith('(')}
    payload = dict(original)
    keys = {n: 'DBFilesClient\\'+n+'.dbc' for n in
            ('Light', 'LightParams', 'LightSkybox', 'LightFloatBand')}
    require({keys[n] for n in ('Light', 'LightParams', 'LightFloatBand')} <= set(payload), f'{city}: lighting input lacks its band tables')
    tables = {n: DBC(payload[k]) for n, k in keys.items() if k in payload}
    light, params, floats = (tables[n] for n in ('Light', 'LightParams', 'LightFloatBand'))
    # The Light row ids are the stable keys; the profile ids are resolved from them (the
    # relighting numbers its new profiles per client). The HD-added rows are all there or none.
    present = light_ids & set(light.index)
    require(present in (light_ids, light_ids-hd_rows), f'{city}: Light rows absent')
    light_ids, profiles = present, {u(light.index[i], 7) for i in present}
    users = {(u(row, 0), col) for row in light.rows for col in range(7, 15)
             if u(row, col) in profiles}
    require(users == {(i, 7) for i in light_ids}, f'{city}: profiles are shared')
    # Verify every affected light's center against authored root-zone cache.
    fog = fog or CLIENT/'world-cache/fog'
    for i in light_ids:
        row = light.index[i]
        require(u(row, 1) == map_id, f'{city}: Light row {i} on another map')
        gx, gy = f(row, 2)/1200, f(row, 4)/1200
        tx, ty = math.floor(gx/16), math.floor(gy/16)
        ix, iy = math.floor(gx) % 16, math.floor(gy) % 16
        data = (fog/f'{map_name}/{tx}_{ty}.frf').read_bytes()
        require(struct.unpack_from('<I', data, 24+4*(iy*16+ix))[0] == zone, f'{city}: Light row {i} outside zone {zone}')
    try:
        clone = hd_sky(city, tables, keys, profiles, hd_profiles, sky_id, custom, omit, batches, removed_count, assets)
        sky_skipped = None
    except Skip as skip:
        if city != 'stormwind':
            raise
        clone, sky_skipped = None, str(skip)
    changed_floats = set()
    new_id = clone['new_id'] if clone else None
    for profile in profiles:
        if clone:
            putu(params.index[profile], 2, new_id)
        if city != 'stormwind':
            continue
        # (a) Denser day fog.
        row = floats.index[(profile-1)*6+1]  # fog end distance only
        old = bytearray(row)
        rewrite(row, True, lambda t, value: value*(1-.1*strength(t)))
        for t in range(2880):
            current, previous = sample_float(row, t), sample_float(old, t)
            assert 0 <= current <= previous+.01
            if t <= 600 or t >= 2400:
                assert abs(current-previous) < .01
        changed_floats.add(u(row, 0))
    if clone:
        payload.update(clone['assets'])
    # Exact scope: old sky rows and all unrelated profiles/bands remain intact.
    for table, allowed in [('LightParams', profiles if clone else set()),
                           ('LightFloatBand', changed_floats), ('LightSkybox', set())]:
        if table not in tables:
            continue
        baseline = DBC(original[keys[table]])
        for id, old in baseline.index.items():
            current = tables[table].index[id]
            if id not in allowed:
                assert current == old
            elif table == 'LightParams':
                assert current[:8] == old[:8] and current[12:] == old[12:]
        payload[keys[table]] = tables[table].bytes()
    changed = sorted(n for n in original if original[n] != payload[n])
    expected = {'LightParams', 'LightSkybox'} if clone else set()
    if city == 'stormwind':
        expected.add('LightFloatBand')
    assert set(changed) == {keys[n] for n in expected}
    output.mkdir(parents=True, exist_ok=True)
    archive = output/'patch-z.mpq'
    with Archive(archive, create=True, capacity=max(16, len(payload)*2)) as a:
        for i, (name, data) in enumerate(payload.items()):
            temp = output/f'payload-{i}.bin'
            temp.write_bytes(data)
            a.add(temp, name)
            temp.unlink()
    with Archive(archive) as a:
        for name, data in payload.items():
            assert a.read(name) == data
    report = dict(city=city, source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
                  profiles=sorted(profiles), light_ids=sorted(light_ids),
                  changed_existing_assets=changed, added_assets=sorted(set(payload)-set(original)),
                  changed_float_bands=sorted(changed_floats),
                  removed_batches=clone['removed'] if clone else [], original_sky=sky_id if clone else None,
                  cloned_sky=new_id, sky_clone_skipped=sky_skipped,
                  unaffected_assets_identical=True, night_fog_preserved=True,
                  game_launched=False, visual_verified=False)
    (output/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


def hd_sky(city, tables, keys, profiles, hd_profiles, sky_id, custom, omit, batches, removed_count, assets):
    """(b) Adds the clone of the city's HD sky without its sun to tables['LightSkybox'] and returns
    {'new_id', 'assets' (the clone's model files), 'removed'}; raises Skip before changing anything
    when the profiles are not the HD ones or the sky, its model or its sun batches are absent."""
    require(profiles == hd_profiles, f'{city}: not the HD profile ids {sorted(hd_profiles)}')
    require('LightSkybox' in tables, f'{city}: lighting input lacks LightSkybox')
    params, sky = tables['LightParams'], tables['LightSkybox']
    require({u(params.index[i], 2) for i in profiles} == {sky_id} and sky_id in sky.index, f'{city}: sky {sky_id} absent')
    row = bytearray(sky.index[sky_id])
    name = sky.strings[u(row, 1):].split(b'\0', 1)[0].decode()
    owned = assets is None
    if owned:
        sys.path.insert(0, str(Path(__file__).resolve().parent/'renderer'))   # this repository's renderer/
        from world_scene_builder import Assets
        assets = Assets()
    added, removed = {}, []
    try:
        require(name[:-4].lower()+'.m2' in assets.providers, f'{city}: sky model {name} absent')
        model = assets.read(name[:-4]+'.m2')
        added[custom+'.m2'] = model  # no vertex, color, animation changes
        count, offset = struct.unpack_from('<II', model, 80)
        textures = []
        for i in range(count):
            kind, flags, size, pos = struct.unpack_from('<4I', model, offset+16*i)
            textures.append(model[pos:pos+size].rstrip(b'\0').decode().lower()
                            if kind == 0 else '')
        require(omit <= {t.split('\\')[-1] for t in textures}, f'{city}: sky {sky_id} has no HD sun batches')
        count, offset = struct.unpack_from('<II', model, 128)
        lookup = struct.unpack_from('<'+'H'*count, model, offset)
        skins = struct.unpack_from('<I', model, 68)[0]
        assert skins == 1
        for i in range(skins):
            before = assets.read(name[:-4]+f'{i:02d}.skin')
            skin = bytearray(before)
            count, offset = struct.unpack_from('<II', skin, 36)
            kept = []
            for j in range(count):
                raw = before[offset+j*24:offset+(j+1)*24]
                batch = struct.unpack('<2B11H', raw)
                n, start = batch[8:10]
                names = [textures[lookup[k]].split('\\')[-1]
                         for k in range(start, start+n)]
                if any(n in omit for n in names):
                    assert set(names) <= omit
                    removed.append({'skin': i, 'batch': j, 'textures': names})
                else:
                    kept.append(raw)
            assert count == batches and len(kept) == batches-removed_count
            assert {n for item in removed for n in item['textures']} == omit
            putu(skin, 9, len(kept))
            skin[offset:offset+24*count] = b''.join(kept)+bytes(24*removed_count)
            assert len(skin) == len(before)
            assert skin[:36] == before[:36]
            assert skin[40:offset] == before[40:offset]
            assert skin[offset+24*count:] == before[offset+24*count:]
            added[custom+f'{i:02d}.skin'] = bytes(skin)
        # Preserve any external animation files if present under the old stem.
        stem = name[:-4].lower()
        for asset in assets.providers:
            if asset.startswith(stem) and asset.endswith('.anim'):
                added[custom+asset[len(stem):]] = assets.read(asset)
    finally:
        if owned:
            assets.close()
    new_id = max(sky.index)+1
    putu(row, 0, new_id)
    putu(row, 1, len(sky.strings))
    sky.strings += (custom+'.mdx').encode()+b'\0'
    sky.add(row)
    return {'new_id': new_id, 'assets': added, 'removed': removed}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--city', choices=['stormwind', 'orgrimmar'], default='stormwind')
    args = parser.parse_args()
    build(args.source, args.output, args.city)
