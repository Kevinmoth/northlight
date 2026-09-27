#!/usr/bin/env python3
# northlight-test: requires=client,stormlib
"""regional_fog_builder does not depend on the client's locale: on the configured client (read only),
four Elwynn Forest tiles (Azeroth 31..32 x 48..49) are built once, then fog is built from them in
enUS and deDE, in the stock view and in the client's own chain. The .frf files must be
byte-identical between the locales and must hold the forest zone (Elwynn Forest, id 12). Before
the forest catalog was proven against AreaTable's enUS name column, which is empty on a deDE
client, so every non-English local build failed at the fog step."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import json, os, shutil, struct, subprocess
import client_archives

client = fp.client_root()
out = fp.output_dir()
cache = out / 'cache'
shutil.rmtree(cache, ignore_errors=True)
env = dict(os.environ, NORTHLIGHT_STORMLIB=str(fp.stormlib()))
locales = [l for l in client_archives.installed_locales(client) if l.lower() in ('enus', 'dede')]
assert len(locales) == 2, f'the client needs enUS and deDE installed (found {locales})'


def run(script, *args):
    r = subprocess.run([sys.executable, '-I', '-B', '-X', 'utf8', str(fp.tracked(script)), '--client', str(client),
                        '--without', 'z', *map(str, args)], env=env, capture_output=True, encoding='utf-8',
                       errors='replace', timeout=1800)
    assert r.returncode == 0, (script, args, r.stdout[-1500:], r.stderr[-1500:])


run('world_scene_builder.py', '--instanced', '--map', 'Azeroth', '--tiles', 31, 48, 32, 49, '--archives', 'stock',
    '--locale', 'enUS', '--output', cache)
tiles = sorted(p.stem for p in (cache / 'Azeroth').glob('*.fg3'))
assert len(tiles) == 4, tiles
for view in ('stock', 'all'):
    fog = {}
    for locale in ('enUS', 'deDE'):
        folder = out / f'fog-{view}-{locale}'
        shutil.rmtree(folder, ignore_errors=True)
        run('regional_fog_builder.py', '--cache', cache, '--output', folder, '--archives', view, '--locale', locale)
        manifest = json.loads((folder / 'manifest.json').read_text())
        azeroth = manifest['maps']['Azeroth']
        assert (azeroth['tiles'], azeroth['expected']) == (4, 4) and not manifest['unsupported'], manifest
        fog[locale] = {p.name: p.read_bytes() for p in sorted((folder / 'Azeroth').glob('*.frf'))}
        print(view, locale, 'zone 12 named', repr(manifest['zones']['12']['name']))
    assert sorted(fog['enUS']) == [t + '.frf' for t in tiles] and fog['enUS'] == fog['deDE'], view
    zones = {z for blob in fog['enUS'].values() for z in struct.unpack_from('<256I', blob, 24)}
    assert 12 in zones, sorted(zones)
shutil.rmtree(cache)
print('fog: byte-identical in enUS and deDE (stock view and full chain), forest zone present')
