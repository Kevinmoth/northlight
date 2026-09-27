vs_3_0
dcl_position0 v0
dcl_texcoord0 v1
dcl_position0 o0
dcl_texcoord0 o1
dcl_texcoord1 o2
mul r0.x, v0.x, c0.x
mul r0.y, v0.y, c1.x
add r0.x, r0.x, r0.y
mul r0.y, v0.z, c2.x
add r0.x, r0.x, r0.y
add r0.x, r0.x, c3.x
mul r0.y, v0.x, c0.y
mul r0.z, v0.y, c1.y
add r0.y, r0.y, r0.z
mul r0.z, v0.z, c2.y
add r0.y, r0.y, r0.z
add r0.y, r0.y, c3.y
mul r0.z, v0.x, c0.z
mul r0.w, v0.y, c1.z
add r0.z, r0.z, r0.w
mul r0.w, v0.z, c2.z
add r0.z, r0.z, r0.w
add r0.z, r0.z, c3.z
mul r0.w, v0.x, c0.w
mul r1.x, v0.y, c1.w
add r0.w, r0.w, r1.x
mul r1.x, v0.z, c2.w
add r0.w, r0.w, r1.x
add r0.w, r0.w, c3.w
mov r1.x, r0.x
mov r1.y, r0.y
mov r1.z, r0.z
mov r1.w, r0.w
mov r0.x, v1.x
mov r0.y, v1.y
mov_sat r0.w, r0.z
mov r1.z, r0.w
mov o0.xyzw, r1.xyzw
mov o1.xy, r0.xyxx
mov o2.x, r0.z
