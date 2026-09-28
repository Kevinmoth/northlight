#!/usr/bin/env python3
# northlight-test:
"""0.3.171 bilinear lighting history (F1, fallback form) and the tighter A1 still gate (F2).

TemporalLight read the previous frame's lighting at the NEAREST history texel. While the camera
moves, a static shadow edge then lags or leads by up to .75 half-res px, with the sign flipping
where the fractional flow crosses .5: bands of opposite shift that move with speed and direction
("waves"). 0.3.171 reads LightHistory (s14) bilinearly at the unrounded reprojection (s14 LINEAR
in the temporal pass only). A per-tap depth test did not fit the slot budget (515 > 500), so the
depth test stays at the nearest texel as before (the decisions' F1-HW fallback).

A 1D emulation of the temporal pass (current, cross box, .75 blend) on static edges, and a
source audit. Reads files only; no Wine, GPU or game. Writes temporal-history-validation.json.
"""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import json
import math
import re

hlsl = fp.src('world_effects.hlsl').read_text()
w = fp.src('world_renderer.h').read_text()
temporal = hlsl[hlsl.index('TemporalOutput TemporalLight('):hlsl.index('// 1x1 pass: fraction of the source disc')]
code = re.sub(r'//[^\n]*', '', temporal)
checks = {}

# ---- 1. source pins ----
NEAREST_TEST = ['    float2 pq=(clamp(floor(puv*half),0,half-1)+.5)/half;',
                '    float hz=tex2Dlod(DepthHistory,float4(pq,0,0)).r;',
                '    if(abs(hz-w)>max(.25,w*.03))return o;']
HISTORY = '    float4 history=tex2Dlod(LightHistory,float4(puv,0,0));'
A1 = ['    float4 tight=clamp(history,lo,hi);',
      '    float still=saturate(2-4*length(puv*half-(base+.5)));',
      '    float margin=.15/max(RemovalInfo.z,1e-4)*RemovalInfo.y*still;',
      '    float loose=clamp(history.a,lo.a-margin,hi.a+margin);',
      '    float3 reach=abs(loose-tight.a)*RemovalInfo.z*TemporalReach.w;',
      '    o.light=lerp(current,float4(clamp(history.rgb,lo.rgb-reach,hi.rgb+reach),loose),TemporalInfo.x);']
checks['TemporalLight: nearest depth test unchanged, history read once at the unrounded puv'] = (
    all(temporal.count(l + '\n') == 1 for l in NEAREST_TEST + [HISTORY])
    and temporal.index(NEAREST_TEST[-1]) < temporal.index(HISTORY)
    and code.count('LightHistory') == 1 and code.count('DepthHistory') == 1 and 'pq,0,0)).r' in code)
checks['A1 lines unchanged except the F2 gate (full to .25 px, none from .5)'] = (
    all(temporal.count(l + '\n') == 1 for l in A1) and 'saturate(2-length(' not in temporal)

# ---- 2. sampler states: s14 LINEAR only in the temporal pass, s15 stays POINT ----
block = w[w.index('{   // Temporal stabilization: light + history'):w.index('temporalIndex=prev;temporalValid=true;')]
loop = ('for(unsigned sampler=14;sampler<16;++sampler){d->SetSamplerState(sampler,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);'
        'd->SetSamplerState(sampler,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);'
        'd->SetSamplerState(sampler,D3DSAMP_MINFILTER,sampler==14?D3DTEXF_LINEAR:D3DTEXF_POINT);'
        'd->SetSamplerState(sampler,D3DSAMP_MAGFILTER,sampler==14?D3DTEXF_LINEAR:D3DTEXF_POINT);'
        'd->SetSamplerState(sampler,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(sampler,D3DSAMP_SRGBTEXTURE,FALSE);}')
normals = ('d->SetTexture(14,normalBuffer);d->SetSamplerState(14,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);'
           'd->SetSamplerState(14,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);d->SetSamplerState(14,D3DSAMP_MINFILTER,D3DTEXF_POINT);'
           'd->SetSamplerState(14,D3DSAMP_MAGFILTER,D3DTEXF_POINT);')
checks['s14 LINEAR (history) and s15 POINT (depth) in the temporal pass; the normals setup resets s14 POINT first'] = (
    block.count(loop) == 1 and w.count(normals) == 1 and w.index(normals) < w.index(block)
    and 'sampler==14?D3DTEXF_LINEAR' not in w.replace(loop, '') and not re.search(r'SetSamplerState\((14|15),D3DSAMP_M(IN|AG)FILTER,D3DTEXF_LINEAR', w))
keys = {'BilinearHistory', 'TemporalHistory', 'HistoryFilter'}
config = [fp.src('quality_settings.h').read_text(), fp.src('windows-package/northlight-quality.ini').read_text(),
          fp.src('windows-package/README.txt').read_text(encoding='utf-8')]
checks['no new key in quality_settings.h, the ini or README'] = not any(k in text for k in keys for text in config)


# ---- 3. 1D emulation: a static edge under a camera moving m half-res px per frame ----
def ramp(x, edge, penumbra):
    return min(max((x - edge) / penumbra + .5, 0), 1)


def history_at(H, p, mode):
    """p: previous-frame screen position in half-res px (texel centres at k+.5)."""
    n = len(H)
    if mode == 'nearest':
        return H[min(max(math.floor(p), 0), n - 1)]
    hp = p - .5
    hb = math.floor(hp)
    hf = hp - hb
    a, b = H[min(max(hb, 0), n - 1)], H[min(max(hb + 1, 0), n - 1)]  # CLAMP addressing
    return a + (b - a) * hf


def crossing(v):
    for i in range(len(v) - 1):
        if v[i] < .5 <= v[i + 1]:
            return i + .5 + (.5 - v[i]) / (v[i + 1] - v[i])
    raise AssertionError('no edge')


def edge_offset(m, penumbra, mode, n=64, frames=300):
    """Mean steady-state offset (half-res px) of the stabilised edge from this frame's edge."""
    edge = n / 2 + (frames - 30) * m  # world position; mid-screen during the measured frames
    H, offsets = None, []
    for frame in range(frames):
        current = [ramp(i + .5 + frame * m, edge, penumbra) for i in range(n)]
        if H is None:
            H = current
            continue
        out = []
        for i in range(n):
            h = history_at(H, i + .5 + m, mode)
            box = current[max(i - 1, 0):i + 2]
            out.append(current[i] + .75 * (min(max(h, min(box)), max(box)) - current[i]))
        H = out
        if frame >= frames - 60:
            offsets.append(crossing(out) - crossing(current))
    return sum(offsets) / len(offsets)


table = {}
for penumbra in (2, 4, 8):
    table[penumbra] = {mode: [edge_offset(k / 10, penumbra, mode) for k in range(1, 10)] for mode in ('nearest', 'bilinear')}
bilinear_max = max(abs(x) for p in table.values() for x in p['bilinear'])
nearest_max = max(abs(x) for p in table.values() for x in p['nearest'])
flips = all(min(p['nearest']) < -.5 and max(p['nearest']) > .5 for p in table.values())
checks[f'moving camera: bilinear edge offset <= .05 px (max {bilinear_max:.3f}); nearest drifted {nearest_max:.2f} px with a sign flip'] = (
    bilinear_max <= .05 and nearest_max >= .5 and flips)

# still camera: the reprojection lands on a texel centre (hf 0) or a hair below the next one
H = [ramp(i + .5, 20.3, 4) for i in range(40)]
still = max(abs(history_at(H, i + .5 + d, 'bilinear') - history_at(H, i + .5 + d, 'nearest'))
            for i in range(40) for d in (0, 1e-6, -1e-6))
checks[f'still camera: bilinear equals the nearest texel within 1e-4 ({still:.1e})'] = still <= 1e-4

# depth rejection: exactly today's nearest-texel rule, current only when it fails
def reject(hz, w):
    return abs(hz - w) > max(.25, w * .03)
checks['depth rejection unchanged: |hz-w| > max(.25, 3% w) returns current only'] = (
    not reject(10.2, 10) and reject(10.4, 10) and not reject(100 + 2.9, 100) and reject(100 + 3.1, 100))

for name, ok in checks.items():
    print(('PASS ' if ok else 'FAIL ') + name)
assert all(checks.values())
out = fp.output_dir()
out.mkdir(parents=True, exist_ok=True)
(out / 'temporal-history-validation.json').write_text(json.dumps({
    'scope': 'Python 1D emulation of TemporalLight history resampling + source audit; no game, Wine or GPU.',
    'checks': checks, 'edge_offset_half_res_px': {str(p): v for p, v in table.items()}}, indent=2) + '\n')
print('PASS temporal history: bilinear history removes the moving-camera edge drift; still camera and rejection unchanged')
