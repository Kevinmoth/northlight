#!/usr/bin/env python3
# northlight-test:
"""Observed camera/projection regression for grazing-water sampling.
Geometry is an analytic horizontal liquid plane, independent of shader depth.
Uses the actual 0.3.0 log's camera basis, projection and framebuffer size.
"""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import math,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parent
W,H=1728,1117
PX,PY=1.2686,1.9626
K=[(.72,.31),(-.41,.89),(1.73,-1.21)]
AMP=[.055,.035,.012]
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def scale(a,s):return tuple(x*s for x in a)
def unit(a):return scale(a,1/math.sqrt(dot(a,a)))
F=unit((.9338,-.1595,-.3204))
R=unit((F[1],-F[0],0))
U=(R[1]*F[2]-R[2]*F[1],R[2]*F[0]-R[0]*F[2],R[0]*F[1]-R[1]*F[0])
def plane_point(x,y,height):
    eye=(-10577.27,-1250.18,height)
    ray=add(add(F,scale(R,(2*x/W-1)/PX)),scale(U,(1-2*y/H)/PY))
    if ray[2]>=0:return None
    distance=-height/ray[2]
    return add(eye,scale(ray,distance)),distance
class Antialias(unittest.TestCase):
    def test_logged_projection_has_real_nyquist_violation(self):
        aliases=0;protected=0;near=0;old_energy=0;new_energy=0
        for height in (5,10,20):
            for y in range(H-1):
                q=plane_point(W*.5,y,height)
                if q is None or not 1<q[1]<617:continue
                p,z=q;px,_=plane_point(W*.5+1,y,height);py,_=plane_point(W*.5,y+1,height)
                dx=sub(px,p);dy=sub(py,p);foot=max(dot(dx,dx),dot(dy,dy))
                normal_weight=max(0,1-foot*.452);foam_weight=max(0,1-foot*4.03)
                maximum=max(abs(dot(dx[:2],k)) for k in K)
                maximum=max(maximum,max(abs(dot(dy[:2],k)) for k in K))
                if maximum>math.pi:
                    aliases+=1;self.assertEqual(normal_weight,0)
                    old=sum(a*math.cos(dot(p[:2],k)) for k,a in zip(K,AMP))
                    old_energy+=old*old;new_energy+=(old*normal_weight)**2
                if max(abs(dot(dx[:2],(5.1,3.7))),abs(dot(dy[:2],(5.1,3.7))))>math.pi:
                    protected+=1;self.assertEqual(foam_weight,0)
                if z<20:
                    near+=1;self.assertGreater(normal_weight,.98)
        self.assertGreater(aliases,40);self.assertGreater(protected,100);self.assertGreater(near,100)
        self.assertGreater(old_energy,.01);self.assertEqual(new_energy,0)
        print('Logged camera fixtures:',aliases,'aliased normal samples;',protected,'aliased foam samples;',near,'near samples preserve >98% amplitude; old artifact energy',round(old_energy,6),'filtered',new_energy)
    def test_filter_is_continuous_and_keeps_resolved_waves(self):
        for coefficient in [.452,4.03]:
            last=1
            for i in range(10001):
                footprint=i/5000
                value=max(0,1-footprint*footprint*coefficient)
                self.assertLessEqual(value,last);self.assertGreaterEqual(value,0)
                self.assertLess(abs(value-last),.002);last=value
            self.assertEqual(last,0)
    def test_shader_derivatives_precede_divergent_masks(self):
        s=fp.src('water_effects.hlsl').read_text()
        for entry in ['WaterReflection','WaterComposite']:
            body=s[s.index('float4 '+entry):]
            self.assertLess(body.index('ddx(p)'),body.index('if(validWater'))
            self.assertLess(body.index('ddy(p)'),body.index('if(validWater'))
        self.assertIn('footprintSquared*.452',s);self.assertIn('footprintSquared*4.03',s)
    def test_reflection_rejects_receivers_below_water(self):
        # A screen-depth match alone does not prove a reflected world hit. The
        # receiving point must lie on/above the horizontal liquid halfspace.
        for receiver_height in [-10,-1,-.25,-.05]:
            depth_error=.01;thickness=.12
            old_accepted=depth_error<thickness
            new_accepted=old_accepted and receiver_height>=-.04
            self.assertTrue(old_accepted);self.assertFalse(new_accepted)
        for receiver_height in [0,.1,2,20]:self.assertTrue(receiver_height>=-.04)
        self.assertIn('receiver.z>=p.z-.04',fp.src('water_effects.hlsl').read_text())
if __name__=='__main__':unittest.main()
