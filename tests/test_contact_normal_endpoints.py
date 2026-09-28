# northlight-test: requires=archive
"""CPU raster regression fitted to an F12 wall/ground capture.

Uses captured camera/projection and planes independently fitted from depth,
not captured erroneous normals. No GPU/game is created. Tests the old and
new smoothing rules against analytic visible-surface normals.
"""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
from build_environment import ARCHIVE
from pathlib import Path
import math,struct,json
from analyze_world_diagnostics import Buffer
ROOT=Path(__file__).resolve().parent
CAP=ARCHIVE/'runtime-diagnostics-0.3.69'
c=list(zip(*[iter(struct.unpack('<272f',(CAP/'capture-1-constants.f32').read_bytes()))]*4))
buf=Buffer(CAP/'capture-1-distance.fgr')
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,s):return tuple(x*s for x in a)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(a):return mul(a,1/math.sqrt(max(dot(a,a),1e-12)))
def agreement(a,b):return dot(a,b)/math.sqrt(max(dot(a,a)*dot(b,b),1e-12))
w,h=map(int,c[33][:2]);sx,sy,sign=c[1][:3]
def ray(x,y):return ((2*x/w-1)/sx,(1-2*y/h)/sy,sign)
def captured_point(x,y):
 px=math.floor((x+.5)*w/buf.width);py=math.floor((y+.5)*h/buf.height)
 return mul(ray(px,py),buf.at(x,y)[0])
def plane(points):
 a,b,d=points;n=unit(cross(sub(b,a),sub(d,a)))
 if dot(n,a)>0:n=mul(n,-1)
 return n,dot(n,a)
ground=plane([captured_point(x,y) for x,y in [(490,268),(510,268),(500,280)]])
wall=plane([captured_point(x,y) for x,y in [(490,230),(510,230),(500,245)]])

# wideNeighbour's acceptance of the wide sample (planar and continuation cosines); .95 since r76
# (was .85). tangentAxis's pair checks below stay .85.
WIDE=.95
def run_region(ground,wall,shift=0,quantize=True):
 planes=[ground,(wall[0],wall[1]+shift)]
 def surface(x,y):
  r=ray(x,y);hits=[]
  for i,(n,d) in enumerate(planes):
   denom=dot(n,r);t=d/denom if abs(denom)>1e-10 else -1
   if t>0:hits.append((t,i))
  return min(hits) if hits else (c[0][3],0)
 def sample(x,y):
  x=max(0,min(w-1,math.floor(x+.5)));y=max(0,min(h-1,math.floor(y+.5)))
  z,_=surface(x,y)
  if quantize:
   near,far=c[0][2:4]
   depth=(far/(far-near))*(1-near/z)/c[2][0]+c[1][3]
   depth=struct.unpack('<f',struct.pack('<f',depth))[0]
   z=near*far/(far-(depth-c[1][3])*c[2][0]*(far-near))
  return mul(ray(x,y),z)
 def normal(x,y,fixed):
  p=sample(x,y);z=p[2]*sign;pixels=max(2,min(40,1.5*sx*w*.5/z));axes=[]
  for step in [(1,0),(0,1)]:
   a1=sample(x-step[0],y-step[1]);b1=sample(x+step[0],y+step[1]);wide=[]
   for direction,near in [(-1,a1),(1,b1)]:
    dx,dy=mul(step,direction);qx=x+dx*pixels;qy=y+dy*pixels
    # Old HLSL reconstructed unsnapped q with the nearest texel's depth.
    q=sample(qx,qy)
    if not fixed:q=mul(ray(qx,qy),q[2]*sign)
    a,b=sub(near,p),sub(q,p)
    valid=abs(q[2]-p[2])<=max(.35,z*.04) and agreement(a,b)>=WIDE
    if fixed:
     qx=math.floor(qx+.5);qy=math.floor(qy+.5)
     valid=valid and agreement(a,sub(q,sample(qx-dx,qy-dy)))>=WIDE
    wide.append(q if valid else near)
   da,db=sub(p,wide[0]),sub(wide[1],p)
   if agreement(da,db)>=.85:t=sub(wide[1],wide[0])
   else:
    da,db=sub(p,a1),sub(b1,p)
    t=sub(b1,a1) if agreement(da,db)>=.85 else (db if abs(b1[2]-p[2])<abs(a1[2]-p[2]) else da)
   axes.append(t)
  n=unit(cross(*axes))
  return mul(n,-1) if dot(n,p)>0 else n
 values=[]
 for x in range(970,1031,5):
  for y in range(475,590):
   z,i=surface(x,y)
   if i!=0:continue
   # Exclude mixed one-pixel silhouettes, the deliberately uncertain path.
   if any(surface(x+dx,y+dy)[1]!=0 for dx,dy in [(1,0),(-1,0),(0,1),(0,-1)]):continue
   errors=[math.degrees(math.acos(max(-1,min(1,dot(normal(x,y,fixed),ground[0]))))) for fixed in [False,True]]
   values.append(errors)
 return values

rows=[]
for shift in [-.3,-.1,0,.1,.3]:rows+=run_region(ground,wall,shift)
old=max(v[0] for v in rows);new=max(v[1] for v in rows)
assert old>15,(old,new)
assert new<1,(old,new)
# r76: synthetic scenes on the captured camera and ground plane. The mirror of WorldNormals'
# fixed path with the wide-sample threshold as a parameter; reports which wide samples it took.
def scene_normal(surface,x,y,limit):
 def sample(x,y):
  x=max(0,min(w-1,math.floor(x+.5)));y=max(0,min(h-1,math.floor(y+.5)))
  near,far=c[0][2:4];z=surface(x,y)
  depth=(far/(far-near))*(1-near/z)/c[2][0]+c[1][3]
  depth=struct.unpack('<f',struct.pack('<f',depth))[0]
  return mul(ray(x,y),near*far/(far-(depth-c[1][3])*c[2][0]*(far-near)))
 p=sample(x,y);z=p[2]*sign;pixels=max(2,min(40,1.5*sx*w*.5/z));axes=[];taken=[]
 for step in [(1,0),(0,1)]:
  a1=sample(x-step[0],y-step[1]);b1=sample(x+step[0],y+step[1]);wide=[]
  for direction,near in [(-1,a1),(1,b1)]:
   dx,dy=mul(step,direction);qx=math.floor(x+dx*pixels+.5);qy=math.floor(y+dy*pixels+.5);q=sample(qx,qy)
   valid=abs(q[2]-p[2])<=max(.35,z*.04) and min(agreement(sub(near,p),sub(q,p)),agreement(sub(near,p),sub(q,sample(qx-dx,qy-dy))))>=limit
   wide.append(q if valid else near);taken.append(valid)
  da,db=sub(p,wide[0]),sub(wide[1],p)
  if agreement(da,db)>=.85:t=sub(wide[1],wide[0])
  else:
   da,db=sub(p,a1),sub(b1,p)
   t=sub(b1,a1) if agreement(da,db)>=.85 else (db if abs(b1[2]-p[2])<abs(a1[2]-p[2]) else da)
  axes.append(t)
 n=unit(cross(*axes));n=mul(n,-1) if dot(n,p)>0 else n
 return math.degrees(math.acos(max(-1,min(1,dot(n,ground[0]))))),taken,pixels
def ground_hit(x,y):
 r=ray(x,y);return mul(r,ground[1]/dot(ground[0],r))
# A far ground pixel (view Z about 28 u) whose upward wide sample spans about 3 u of ground.
X0,Y0=1000,300
P=ground_hit(X0,Y0);_,_,WIDE_PX=scene_normal(lambda x,y:ground_hit(x,y)[2]*sign,X0,Y0,WIDE)
U=unit(sub(ground_hit(X0,Y0-WIDE_PX),P))  # along the ground toward the upward wide sample
def height(X):return dot(ground[0],X)-ground[1]
def box_scene(high,distance,depth=4.):
 """Flat ground and a box `high` u tall whose front face stands `distance` u from P toward the wide sample."""
 m=mul(U,-1);md=dot(m,add(P,mul(U,distance)))
 def surface(x,y):
  r=ray(x,y);best=ground[1]/dot(ground[0],r)
  den=dot(m,r)
  if abs(den)>1e-10:
   tw=md/den
   if 0<tw<best and 0<=height(mul(r,tw))<=high:best=tw
  tt=(ground[1]+high)/dot(ground[0],r)
  if 0<tt<best and distance<=dot(U,sub(mul(r,tt),P))<=distance+depth:best=tt
  return best
 return surface
# Along screen x the ground keeps its view depth, so the wide sample passes the depth test on
# gentle hills (upward, a flat 3 u sample is already beyond max(.35,.04z)).
V=unit(sub(ground_hit(X0+WIDE_PX,Y0),P))
def hill_scene(bend,crease=.3):
 """Flat ground that bends up by `bend` degrees along a crease `crease` u toward the +x wide sample."""
 k=math.radians(bend);up=add(mul(ground[0],math.cos(k)),mul(V,-math.sin(k)))
 C=add(P,mul(V,crease));dfar=dot(up,C)
 def surface(x,y):
  r=ray(x,y);t=ground[1]/dot(ground[0],r)
  return dfar/dot(up,r) if dot(V,sub(mul(r,t),C))>0 else t
 return surface
box=box_scene(.6,1.2)
assert box(X0,Y0-round(WIDE_PX))<ground_hit(X0,Y0-round(WIDE_PX))[2]*sign  # the wide sample lands on the object's top
base={limit:scene_normal(box,X0,Y0,limit) for limit in (.85,.95)}
assert base[.85][0]>10 and base[.95][0]<1,base  # a .6 u object 1.2 u away: 0.3.174 tilted the ground, r76 does not
# At this grazing view (about 21 degrees down) a bend across the screen row reaches the 1-px
# continuation test amplified about 2.7x, so the acceptance limit is a few degrees of real bend
# (.95: about 6, .85: about 11); gentle bends inside it keep the wide sample.
hill={bend:scene_normal(hill_scene(bend),X0,Y0,WIDE) for bend in (3,5)}
assert all(taken[1] for _,taken,_ in hill.values()),hill  # gentle bends keep the +x wide sample
limit_bend={limit:max(b/2 for b in range(0,61) if scene_normal(hill_scene(b/2),X0,Y0,limit)[1][1]) for limit in (.85,.95)}
# Analytic near/far raster tests also cover signed view Z through the existing
# raster reconstruction regression. This fixture targets the captured LH view.
nb=Buffer(CAP/'capture-1-normals.fgr');lb=Buffer(CAP/'capture-1-light.fgr')
a=nb.at(500,259)[:3];b=nb.at(500,265)[:3]
assert all(lb.at(500,y)[3]==0 for y in range(250,280))
report={'status':'PASS','capture':'0.3.69/capture-1','game_or_gpu_launched':False,
 'surface_samples':len(rows),'captured_shadow_visibility_at_seam':0,
 'captured_normal_jump_degrees':math.degrees(math.acos(max(-1,min(1,dot(unit(a),unit(b)))))),
 'fitted_plane_regression':{'old_max_normal_error_degrees':old,'new_max_normal_error_degrees':new},
 'object_06u_at_12u_normal_tilt_degrees':{'0.85':base[.85][0],'0.95':base[.95][0]},
 'hill_x_wide_sample_taken':{str(k):v[1][1] for k,v in hill.items()},
 'largest_hill_bend_keeping_the_wide_sample_degrees':{str(k):v for k,v in limit_bend.items()},
 'scope':'CPU planes fitted to capture, five wall offsets; patched GPU visual confirmation pending'}
print(json.dumps(report,indent=2))
