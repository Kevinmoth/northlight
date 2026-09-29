"""Build a 3.3.5a data-only outdoor relighting patch, based on this HD client.

Schemas: WoWDBDefs; band meanings/indexing: Noggit RED Sky.h/Sky.cpp.
All existing rows are preserved. New profiles are referenced only by outdoor
clear-weather Light rows, so shared dungeon/underwater/weather profiles retain
their original data. This is authored legacy lighting, not global illumination.
"""
import copy
from collections import Counter
import hashlib
import json
import math
import struct
import tempfile
from pathlib import Path
from mpq import Archive, ROOT

SOURCE = ROOT / 'inspection/patch-x.mpq'
OUTPUT = ROOT / 'build/lighting'
MAPS = {0: 'Eastern Kingdoms', 1: 'Kalimdor', 530: 'Outland', 571: 'Northrend'}
TABLES = ('Light', 'LightParams', 'LightIntBand', 'LightFloatBand')

class Skip(Exception):
    """An input lacks what an HD-specific step needs (a sky id, profile or texture): skip that step."""

def require(condition, reason):
    if not condition:
        raise Skip(reason)

class DBC:
    def __init__(self, data):
        magic, count, self.fields, self.size, strings = struct.unpack_from('<4s4I', data)
        assert magic == b'WDBC' and self.size == self.fields*4
        assert len(data) == 20+count*self.size+strings
        self.rows = [bytearray(data[20+i*self.size:20+(i+1)*self.size]) for i in range(count)]
        self.strings = data[20+count*self.size:]
        self.index = {u(r, 0): r for r in self.rows}
        assert len(self.index) == count

    def add(self, row):
        assert len(row) == self.size and u(row,0) not in self.index
        self.rows.append(row)
        self.index[u(row,0)] = row

    def bytes(self):
        return struct.pack('<4s4I', b'WDBC', len(self.rows), self.fields, self.size,
                           len(self.strings)) + b''.join(self.rows) + self.strings

def u(r, col): return struct.unpack_from('<I', r, col*4)[0]
def f(r, col): return struct.unpack_from('<f', r, col*4)[0]
def putu(r, col, value): struct.pack_into('<I', r, col*4, value)
def putf(r, col, value):
    assert math.isfinite(value)
    struct.pack_into('<f', r, col*4, value)
def clamp(x, lo, hi): return max(lo, min(x, hi))
def rgb(v): return [(v>>16)&255, (v>>8)&255, v&255]
def pack(c, original):
    r,g,b = [round(clamp(x, 0, 255)) for x in c]
    return (original & 0xff000000) | (r<<16) | (g<<8) | b
def mix(a,b,t): return [x*(1-t)+y*t for x,y in zip(a,b)]
def lum(c): return c[0]*.2126+c[1]*.7152+c[2]*.0722

def sample_color(row, time):
    n = u(row,1)
    if not n: return None
    points = sorted((u(row,2+i), rgb(u(row,18+i))) for i in range(n))
    if n == 1: return points[0][1]
    points = [(points[-1][0]-2880, points[-1][1])] + points + [(points[0][0]+2880, points[0][1])]
    for (t0,c0),(t1,c1) in zip(points,points[1:]):
        if t0 <= time <= t1:
            return mix(c0,c1,(time-t0)/max(1,t1-t0))
    raise ValueError('Invalid time band')

def daylight(time):
    # Existing WoW key times use half-minutes. Smooth transitions avoid jumps.
    return clamp((math.cos((time-1560)*2*math.pi/2880)+.2)/.9,0,1)

def sort_band(row):
    # Some input bands are unsorted. Keep each time/value pair together.
    pairs=sorted((u(row,2+i),u(row,18+i)) for i in range(u(row,1)))
    for i,(time,value) in enumerate(pairs):
        putu(row,2+i,time); putu(row,18+i,value)

def transform_color(ch, original, time, count, sun):
    c = rgb(original)
    day = daylight(time) if count > 1 else 1.0
    if ch == 0:  # Diffuse/key light: less monochromatic HD sunlight.
        if sun and lum(sun)>0:
            target = [v*lum(c)/lum(sun) for v in sun]
            c = mix(c,target,.28*day)
        c = [v*(1.04+.08*day) for v in c]
    elif ch == 1:  # Ambient fill: stronger shape, still readable at night.
        c = [v*(.82-.04*day) for v in c]
        if lum(c)>0 and day<1:
            cool = [0.85,0.97,1.10]
            c = [v*(day+(1-day)*k) for v,k in zip(c,cool)]
    elif ch in (2,3,4):
        c = [v*(.90+.06*day) for v in c]
    elif ch == 7:  # Preserve each zone's fog hue.
        c = [v*(.90+.05*day) for v in c]
    elif ch == 8:  # Existing terrain shadow mask opacity, NOT a new shadow map.
        c = [clamp(v*(1.05+.23*day),0,160) for v in c]
    elif ch == 10:
        c = [v*.92 for v in c]
    elif ch in (14,16):
        c = [v*.95 for v in c]
    elif ch in (15,17):
        c = [v*.80 for v in c]
    return pack(c,original)

def relight(tables):
    """The outdoor relighting, in place on {name: DBC} for TABLES; returns (changes, skipped, zones)."""
    light, params, ints, floats = [tables[n] for n in TABLES]
    profile_ids = sorted({u(r,7) for r in light.rows if u(r,1) in MAPS and u(r,7)})
    # Reserve beyond ALL existing band IDs, including unused/incomplete profiles.
    next_id = max(max(params.index), (max(ints.index)+17)//18, (max(floats.index)+5)//6)+1
    mapping, changes, skipped = {}, {}, []
    for old in profile_ids:
        if old not in params.index or any((old-1)*18+c+1 not in ints.index for c in range(18)) or any((old-1)*6+c+1 not in floats.index for c in range(6)):
            skipped.append(old); continue
        new = next_id; next_id += 1; mapping[old] = new
        row = copy.copy(params.index[old]); putu(row,0,new)
        # Preserve skybox references and opaque flags, including this pack's
        # nonstandard last field. Tune only documented opacity/glow floats.
        glow=f(row,3)
        if 0<glow<=1: putf(row,3,min(glow*.8,.28))
        for col in (4,6):
            value=f(row,col)
            if 0<value<=1: putf(row,col,max(.12,value*.78))
        for col in (5,7):
            value=f(row,col)
            if 0<value<=1: putf(row,col,min(.92,value+.08))
        params.add(row)
        modifications=0
        sun=ints.index[(old-1)*18+10]
        for ch in range(18):
            row=copy.copy(ints.index[(old-1)*18+ch+1]); putu(row,0,(new-1)*18+ch+1)
            for i in range(u(row,1)):
                t=u(row,2+i); before=u(row,18+i)
                after=transform_color(ch,before,t,u(row,1),sample_color(sun,t))
                putu(row,18+i,after); modifications += before!=after
            sort_band(row); ints.add(row)
        for ch in range(6):
            row=copy.copy(floats.index[(old-1)*6+ch+1]); putu(row,0,(new-1)*6+ch+1)
            for i in range(u(row,1)):
                value=f(row,18+i); day=daylight(u(row,2+i)) if u(row,1)>1 else 1.0
                if ch==0 and value>=3600: # >=100 yards; retain tight local fog volumes.
                    putf(row,18+i,value*(.94+.16*day))
                elif ch==1 and 0<value<1:
                    putf(row,18+i,clamp(value*.88,0,.95))
            sort_band(row); floats.add(row)
        changes[old]={'new':new,'color_keys_changed':modifications}
    zones={name:0 for name in MAPS.values()}
    for row in light.rows:
        if u(row,1) in MAPS and u(row,7) in mapping:
            putu(row,7,mapping[u(row,7)]); zones[MAPS[u(row,1)]]+=1
    return changes, skipped, zones

# Retime (0.3.178): the native bands of relight's private clear-weather profiles, moved onto
# Northlight's sun and moon. Times are half-minutes (0-2879). Only times move; the (relit) values
# stay, so the colour of every key is kept and only when it is reached changes.
SUNSET_KEY, NIGHT_KEY, DAWN_KEY = 2430, 2520, 540   # 20:15 sunset colour, 21:00 night, 04:30 dawn start
INSERT_LIMIT = 11   # Mulgore and Stormwind add up to 5 keys afterwards, under the 16-key limit

def band_pairs(row, floating):
    return [(u(row,2+i), f(row,18+i) if floating else u(row,18+i)) for i in range(u(row,1))]

def band_value(pairs, floating, time):
    """The band's (interpolated, wrapping) value at `time`: rgb for ints, a float for floats."""
    points = [(t, v if floating else rgb(v)) for t, v in pairs]
    if len(points) == 1: return points[0][1]
    points = [(points[-1][0]-2880, points[-1][1])] + points + [(points[0][0]+2880, points[0][1])]
    for (t0,a),(t1,b) in zip(points, points[1:]):
        if t0 <= time <= t1:
            w = (time-t0)/max(1, t1-t0)
            return a+(b-a)*w if floating else mix(a, b, w)
    raise ValueError('Invalid time band')

def band_difference(a, b, floating):
    return abs(a-b) if floating else max(abs(x-y) for x, y in zip(a, b))

def retime_plan(row, floating):
    """(new pairs, reason): reason is None for a retimed band, else why it is left byte-identical."""
    pairs = band_pairs(row, floating)
    times = [t for t, _ in pairs]
    if len(set(times)) < 3: return None, 'fewer than 3 key times'
    night = band_value(pairs, floating, 0)
    value = lambda v: v if floating else rgb(v)
    spread = max(band_difference(value(v), night, floating) for _, v in pairs)
    tolerance = max(1e-3, .1*spread) if floating else max(3, .1*spread)
    if spread <= (1e-3 if floating else 3): return None, 'constant'
    dark = lambda v: band_difference(v, night, floating) <= tolerance
    # Evening: N_old is the first key from 18:00 on after which every key is night; S_old the key before.
    night_start = next((t for i, (t, _) in enumerate(pairs) if t >= 2160 and all(dark(value(v)) for _, v in pairs[i:])), 2880)
    before = [(t, v) for t, v in pairs if t < night_start]
    if not before or before[-1][0] < 1920 or dark(value(before[-1][1])): return None, 'evening shape not recognised'
    sunset = before[-1][0]
    # Morning: M_old is the last key before 06:00 that ends an all-night run from 00:00.
    dawn = 0
    for t, v in pairs:
        if t >= 720 or not dark(value(v)): break
        dawn = t
    if dark(band_value(pairs, floating, 720)): return None, 'morning shape not recognised'
    def move(t):
        if dawn < DAWN_KEY and t <= 720:
            if t < dawn: return t*DAWN_KEY/dawn
            if t == dawn: return DAWN_KEY if dawn else 0   # dawn 0: 00:00 stays, a night key is inserted at 04:30
            return DAWN_KEY+(t-dawn)*(720-DAWN_KEY)/(720-dawn)
        if t <= 1440: return t
        if t <= sunset: return 1440+(t-1440)*(SUNSET_KEY-1440)/(sunset-1440)
        if t <= night_start: return SUNSET_KEY+(t-sunset)*(NIGHT_KEY-SUNSET_KEY)/(night_start-sunset)
        return NIGHT_KEY+(t-night_start)*(2880-NIGHT_KEY)/(2880-night_start)
    moved = [(round(move(t)), v) for t, v in pairs]
    # The night value, as a key: the stored 00:00 key when there is one (keeps an int band's alpha byte).
    key = next((v for t, v in pairs if t == 0), night if floating else pack(night, pairs[0][1]))
    inserts, skipped = [], []
    for t, needed in ((NIGHT_KEY, night_start == 2880), (DAWN_KEY, dawn == 0)):
        if needed: (inserts if len(moved)+len(inserts) < INSERT_LIMIT else skipped).append(t)
    result = sorted(moved+[(t, key) for t in inserts], key=lambda p: p[0])
    if any(a >= b for (a, _), (b, _) in zip(result, result[1:])) or result[-1][0] >= 2880: return None, 'rounding would merge keys'
    return result, ('inserts skipped (key limit): ' + ','.join(map(str, skipped))) if skipped else None

def retime(tables, profiles):
    """Retime every band of `profiles` (relight's new profile ids) in place; returns the report."""
    report = {'bands': 0, 'retimed': 0, 'inserted_keys': 0, 'insert_skipped': [], 'unchanged': {}, 'templates': {}, 'rows': {}}
    for name, floating, channels in (('LightIntBand', False, 18), ('LightFloatBand', True, 6)):
        table = tables[name]
        for profile in sorted(profiles):
            for ch in range(channels):
                row = table.index[(profile-1)*channels+ch+1]
                before = band_pairs(row, floating)
                template = ' '.join(f'{t//120:02d}' + (f':{t%120//2:02d}' if t % 120 else '') for t, _ in before)
                result, reason = retime_plan(row, floating)
                report['bands'] += 1
                entry = report['templates'].setdefault(template, {'bands': 0})
                entry['bands'] += 1
                if result is None:
                    report['unchanged'][reason] = report['unchanged'].get(reason, 0)+1
                    entry[reason] = entry.get(reason, 0)+1
                    continue
                if reason: report['insert_skipped'].append({'table': name, 'id': u(row,0), 'template': template})
                # Invariants: the original values survive as a multiset, inserts copy the night value,
                # times are unique, increasing and in range, and 06:00-12:00 is untouched.
                extra = Counter(v for _, v in result); extra.subtract(v for _, v in before)
                assert all(n >= 0 for n in extra.values()) and set(extra.elements()) <= {v for t, v in result if t in (NIGHT_KEY, DAWN_KEY)}
                assert all(b > a for (a, _), (b, _) in zip(result, result[1:])) and 0 <= result[0][0] and result[-1][0] < 2880
                assert len(result) <= 16 and (len(result) == len(before) or len(result) <= INSERT_LIMIT)
                assert [p for p in result if 720 <= p[0] <= 1440] == [p for p in before if 720 <= p[0] <= 1440]
                putu(row, 1, len(result))
                for i in range(16):
                    putu(row, 2+i, result[i][0] if i < len(result) else 0)
                    if floating: putf(row, 18+i, result[i][1] if i < len(result) else 0.)
                    else: putu(row, 18+i, result[i][1] if i < len(result) else 0)
                report['retimed'] += 1
                report['inserted_keys'] += len(result)-len(before)
                entry['retimed'] = entry.get('retimed', 0)+1
                report['rows'][f'{name}:{u(row,0)}'] = {'before': [t for t, _ in before], 'after': [t for t, _ in result]}
    return report

def build():
    tables = {n: DBC((SOURCE/(n+'.dbc')).read_bytes()) for n in TABLES}
    original_rows = {n:len(t.rows) for n,t in tables.items()}
    changes, skipped, zones = relight(tables)
    OUTPUT.mkdir(parents=True,exist_ok=True)
    report={'source':'data/patch-x.mpq (identical to enUS sky patch)',
            'profiles':changes,'skipped_incomplete_profiles':skipped,
            'light_volumes':zones,'tables':{},'runtime_validated':False}
    for name,table in tables.items():
        data=table.bytes(); DBC(data)
        path=OUTPUT/(name+'.dbc');path.write_bytes(data)
        report['tables'][name]={'old_rows':original_rows[name],'new_rows':len(table.rows),
                               'sha256':hashlib.sha256(data).hexdigest()}
    (OUTPUT/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    archive_path=ROOT/'build/patch-z.mpq'
    with tempfile.TemporaryDirectory(dir=ROOT/'build') as temp:
        staging=Path(temp)/'patch-z.mpq'
        with Archive(staging,create=True,capacity=16) as archive:
            for name in tables: archive.add(OUTPUT/(name+'.dbc'),'DBFilesClient\\'+name+'.dbc')
        staging.replace(archive_path)
    with Archive(archive_path) as archive:
        for name in tables:
            assert archive.read('DBFilesClient\\'+name+'.dbc')==(OUTPUT/(name+'.dbc')).read_bytes()
    print(json.dumps({'profiles':len(changes),'light_volumes':zones,'tables':report['tables'],'skipped':skipped},indent=2))

if __name__=='__main__': build()
