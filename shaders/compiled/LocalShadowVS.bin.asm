vs_3_0
dcl_position0 v0
dcl_texcoord0 v1
dcl_position0 o0
dcl_texcoord0 o1
dcl_texcoord7 o2
mul r0.xyzw, v0.x, c0.xyzw
mul r1.xyzw, v0.y, c1.xyzw
add r0.xyzw, r0.xyzw, r1.xyzw
mul r1.xyzw, v0.z, c2.xyzw
add r0.xyzw, r0.xyzw, r1.xyzw
add r0.xyzw, r0.xyzw, c3.xyzw
mov o0.xyzw, r0.xyzw
mov o1.xy, v1.xyxx
mov o2.xyzw, r0.xyzw
