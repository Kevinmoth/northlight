ps_3_0
dcl_texcoord0 v0
dcl_texcoord7 v1
def c10 = 0.00000000e+00, 1.00000000e+00, 0.00000000e+00, 0.00000000e+00
dcl_2d s0
mul r0.x, c1.y, v1.w
add r0.y, v1.x, r0.x
add r0.x, v1.y, -r0.x
mov r0.z, r0.x
mov r0.w, v1.w
mov r1.xyz, r0.yzwy
mov r0.xyz, r0.yzwy
dp3 r0.x, r1.xyzx, r0.xyzx
add r0.x, c1.x, -r0.x
texkill r0.x
cmp r0.x, c0.w, c10.x, c10.y
add r0.x, -r0.x, c10.y
if_ne r0.x, -r0.x
    texld r0.xyzw, v0.xyxx, s0
    add r0.x, r0.w, -c0.w
    texkill r0.x
else
endif
rcp r0.x, v1.w
mul r0.x, v1.z, r0.x
mov_sat r0.x, r0.x
mov oC0.xyzw, r0.x
