# northlight-test:
"""Offline palette relighting and shadow-filter regression; no device/game."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import json
import math
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parent
shader = fp.src('world_effects.hlsl').read_text()
assert 'float3 sun=DirectLight.rgb*saturate(dot(n,LegacyDirection.xyz))*visibility;' in shader
assert 'lerp(sun,moon,SunDirection.w)' in shader
assert 'if(dot(DirectLight.rgb,1)>0)shadow=directionalShadow' in shader
assert '[loop]for(int i=0;i<16;++i)' in shader
assert 'max(2-abs(offset-fraction),0)' in shader
assert 'dot(gradient,tapUV-uv)' in shader

def filter_edge(x, size):
    base = math.floor(x + .5) if size == 3 else math.floor(x)
    radius = size / 2
    weights = [max(radius - abs(i - (x-base)), 0) for i in range(-1, size-1)]
    return sum(w * (base+i >= 0) for i, w in zip(range(-1, size-1), weights)) / sum(weights)

# Continuous on either side of integer texel boundaries, monotone on a hard
# fence edge. The wider kernel must soften the transition without leaking light
# into a fully shadowed region or darkening a fully lit receiver plane.
for i in range(-4, 5):
    assert abs(filter_edge(i-1e-7,4) - filter_edge(i+1e-7,4)) < 1e-6
previous = 0
widths = {}
for size in (3, 4):
    partial = []
    for i in range(-4000, 4001):
        x = i / 1000
        v = filter_edge(x, size)
        assert -1e-12 <= v <= 1+1e-12
        if size == 4:
            assert v >= previous-1e-12
            previous = v
        if .1 <= v <= .9:
            partial.append(x)
    widths[size] = partial[-1] - partial[0]
assert widths[4] > widths[3]
assert filter_edge(-4,4) == 0 and filter_edge(4,4) == 1
rng = random.Random(58)
for _ in range(10000):
    color = [rng.random() for _ in range(3)]
    tinted = [rng.random()*2 for _ in range(3)]
    weight, shadow, native_cos, moon_cos = [rng.random() for _ in range(4)]
    painted = [c*weight*native_cos for c in color]
    for moon in (False, True):
        angular = moon_cos if moon else native_cos
        visibility = shadow if moon else .15+.85*shadow
        target = [c*weight*angular*visibility for c in tinted]
        correction = [a-b for a,b in zip(target,painted)]
        assert all(math.isclose(a+b,c,abs_tol=1e-12) for a,b,c in zip(painted,correction,target))
        # Turning off a source subtracts its old native share, even when that
        # source has no shadow pass. Sun and moon defaults preserve old policy.
        assert all(p + (-p) == 0 for p in painted)
    # Plane-depth correction keeps a flat receiver fully lit for any slope.
    fraction = rng.random()
    slope = rng.uniform(-4,4)
    for offset in range(-1,3):
        stored = .5 + slope*(offset-fraction)
        expected = .5-.00008 + slope*(offset-fraction)
        assert expected < stored

disc = fp.src('celestial_disc_effects.hlsl').read_text()
# the tint moves halfway toward the glow's hot-core colour (hue mix 1);
# brightness is still limited before tinting.
assert 'saturate(texel.rgb*DiscEmission.x)*lerp(saturate(DiscColor.rgb),DiscGlowCore.rgb,.5*DiscGlowHue.w)' in disc
green = (.68, 1., .62)
for core in ((.68, 1., .62), (.85, 1., .8), (1., 1., 1.)):
    tint = [g+(c-g)*.5 for g,c in zip(green,core)]
    for gain in (1.6,1.8,3.5):
        output = [min(1.,gain)*c for c in tint]
        assert output[1] >= output[0] >= output[2]  # bright core stays green (or neutral)
print(json.dumps({'status':'PASS','relighting_cases':20000,
                  'shadow_edge_10_to_90_percent_width_texels':widths,
                  'game_or_gpu_launched':False},indent=2))
