vs_3_0
dcl_position0 v0
dcl_texcoord0 v1
dcl_texcoord1 v2
dcl_texcoord2 v3
dcl_texcoord3 v4
dcl_position0 o0
dcl_texcoord0 o1
dcl_texcoord1 o2
def c7 = 1.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
mov r0.xyz, v0.xyzx
mov r0.w, c7.x
mov r1.xyzw, r0.xyzw
dp4 r1.x, r1.xyzw, v2.xyzw
mov r2.xyzw, r0.xyzw
dp4 r1.y, r2.xyzw, v3.xyzw
dp4 r0.x, r0.xyzw, v4.xyzw
mul r0.y, r1.x, c0.x
mul r0.z, r1.y, c1.x
add r0.y, r0.y, r0.z
mul r0.z, r0.x, c2.x
add r0.y, r0.y, r0.z
add r0.y, r0.y, c3.x
mul r0.z, r1.x, c0.y
mul r0.w, r1.y, c1.y
add r0.z, r0.z, r0.w
mul r0.w, r0.x, c2.y
add r0.z, r0.z, r0.w
add r0.z, r0.z, c3.y
mul r0.w, r1.x, c0.z
mul r1.z, r1.y, c1.z
add r0.w, r0.w, r1.z
mul r1.z, r0.x, c2.z
add r0.w, r0.w, r1.z
add r0.w, r0.w, c3.z
mul r1.x, r1.x, c0.w
mul r1.y, r1.y, c1.w
add r1.x, r1.x, r1.y
mul r0.x, r0.x, c2.w
add r0.x, r1.x, r0.x
add r0.x, r0.x, c3.w
mov r1.x, r0.y
mov r1.y, r0.z
mov r1.z, r0.w
mov r1.w, r0.x
mov_sat r0.x, r0.w
mov r1.z, r0.x
mov o0.xyzw, r1.xyzw
mov o1.xy, v1.xyxx
mov o2.x, r0.w
