#!/usr/bin/env python3
# northlight-test:
"""Finite analytic-world fixtures for the liquid depth/mask contract.
Independent world-space ray/plane intersections are the expected answers.
Shader reconstruction is tested against them, including D24 storage and FP32 math.
No game, device, or image generation is used.
"""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import math,struct,unittest,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent

def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def half(x):return struct.unpack('<e',struct.pack('<e',x))[0]
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def scale(a,s):return tuple(x*s for x in a)
def sub(a,b):return add(a,scale(b,-1))
def unit(a):return scale(a,1/math.sqrt(dot(a,a)))
def store_depth(z,near,far,minz=0,maxz=1):
    d=(far-far*near/z)/(far-near)
    return f32(round((minz+d*(maxz-minz))*16777215)/16777215)
def reconstruct_depth(raw,near,far,minz=0,maxz=1):
    n=f32(near);f=f32(far);d=max(0,min(1,f32(f32(raw-f32(minz))*f32(1/(maxz-minz)))))
    return f32(f32(n*f)/max(f32(f-f32(d*f32(f-n))),.00001))
def visible(mask_z,alpha,opaque_z,near):return mask_z>near and mask_z<=opaque_z+max(.012,mask_z*.0007) and alpha>.003
def foam_candidate(water_z,floor_z,water_world,floor_world,near):
    quantization=water_z*water_z*(4/16777215)/near
    gap=water_world[2]-floor_world[2]
    return floor_z-water_z>quantization and .015<gap<.8
class Geometry(unittest.TestCase):
    def test_geometry_mask_excludes_land_and_foreground(self):
        # Camera sees an actual rectangular liquid mesh. An infinite sloped
        # terrain plane crosses it at x=0; positive-x terrain occludes water.
        eye=(0.,0.,6.);pitch=.34
        right=(1.,0.,0.);up=(0.,math.sin(pitch),math.cos(pitch));forward=(0.,math.cos(pitch),-math.sin(pitch))
        near=.5;far=1000;counts={'wet':0,'occluded_land':0,'no_geometry':0,'shore':0,'deep':0}
        for y in range(1,96):
            for x in range(1,160):
                ray=add(add(scale(right,(2*x/160-1)/1.2),scale(up,(1-2*y/96)/1.7)),forward)
                if ray[2]>=-.001:continue
                water_z=-eye[2]/ray[2];water=add(eye,scale(ray,water_z))
                denominator=ray[2]-.2*ray[0]
                if abs(denominator)<.001:continue
                floor_z=-eye[2]/denominator
                if floor_z<=near:continue
                floor_point=add(eye,scale(ray,floor_z));in_mesh=-8<=water[0]<=8 and 8<=water[1]<=40
                mask=f32(water_z) if in_mesh else 0.;depth=reconstruct_depth(store_depth(floor_z,near,far),near,far)
                active=visible(mask,1 if in_mesh else 0,depth,near)
                if not in_mesh:counts['no_geometry']+=1;self.assertFalse(active)
                elif floor_point[2]>.08:counts['occluded_land']+=1;self.assertFalse(active)
                elif floor_point[2]<-.08:
                    counts['wet']+=1;self.assertTrue(active)
                    reconstructed_floor=add(eye,scale(ray,depth));candidate=foam_candidate(mask,depth,water,reconstructed_floor,near)
                    if -.7<floor_point[2]<-.08:counts['shore']+=1;self.assertTrue(candidate)
                    if floor_point[2]<-.9:counts['deep']+=1;self.assertFalse(candidate)
        self.assertTrue(all(v>20 for v in counts.values()),counts)
        self.assertFalse(visible(12,0,30,near)) # retained original transparent alpha
        self.assertFalse(visible(12,1,8,near)) # opaque actor in front
        self.assertTrue(visible(12,1,18,near)) # submerged receiver behind
        self.fixture_counts=counts
    def test_signed_projection_reconstructs_analytic_world(self):
        # Same world point seen from distinct cameras must yield the same wave
        # anchor; test LH/RH view Z and mirrored projection axes independently.
        expected=(-9996.2,-479.7,22.35)
        for eye in [(-10000.,-500.,28.),(-9995.,-508.,33.)]:
            pitch=.4;right=(1.,0.,0.);up=(0.,math.sin(pitch),math.cos(pitch));forward=(0.,math.cos(pitch),-math.sin(pitch));relative=sub(expected,eye)
            z=dot(relative,forward)
            for sign in [-1,1]:
                for px in [-1.4,1.4]:
                    py=1.7;w=1920;h=1080
                    uv=(dot(relative,right)*px/z*.5+.5+.5/w,.5-dot(relative,up)*py/z*.5+.5/h)
                    view=((2*(uv[0]-.5/w)-1)/px*f32(z),(1-2*(uv[1]-.5/h))/py*f32(z),sign*f32(z))
                    result=add(eye,add(add(scale(right,view[0]),scale(up,view[1])),scale(forward,view[2]*sign)))
                    self.assertLess(math.sqrt(dot(sub(result,expected),sub(result,expected))),.0001)
    def test_own_water_depth_never_creates_shore_foam(self):
        tested=0
        for near in [.1,.5,1.]:
            for far in [500.,1000.,5000.]:
                for minz,maxz in [(0.,1.),(.001,.9999),(0.,.99)]:
                    for i in range(1,1000):
                        z=near*2*(far*.95/(near*2))**(i/1000)
                        mask=f32(z);floor=reconstruct_depth(store_depth(z,near,far,minz,maxz),near,far,minz,maxz)
                        for down in [.15,.5,.95]:
                            water_world=(0,0,-mask*down);floor_world=(0,0,-floor*down)
                            self.assertFalse(foam_candidate(mask,floor,water_world,floor_world,near),(near,far,z,mask,floor,down))
                            tested+=1
        self.assertGreater(tested,80000)
    def test_old_fp16_failure_is_detected(self):
        # A real quantization regression: same water surface in mask and depth
        # used to look like a shoreline because half storage moved its plane.
        z=256.12;old_mask=half(z);floor=reconstruct_depth(store_depth(z,.5,1000),.5,1000)
        old_gap=(floor-old_mask)*.8
        self.assertTrue(.015<old_gap<.8,(old_mask,floor,old_gap))
        new_mask=f32(z);self.assertFalse(foam_candidate(new_mask,floor,(0,0,-new_mask*.8),(0,0,-floor*.8),.5))
    def test_hlsl_contract_matches_validated_precision(self):
        hlsl=fp.src('water_effects.hlsl').read_text();header=fp.src('water_renderer.h').read_text()
        self.assertIn('D3DFMT_G32R32F,mask,maskSurface',header)
        self.assertIn('m.g>.003',hlsl);self.assertIn('mask.g*4',hlsl)
        self.assertIn('4.0/16777215.0',hlsl)
if __name__=='__main__':unittest.main()
