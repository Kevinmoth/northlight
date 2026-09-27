ps_3_0
dcl_texcoord0 v0
def c68 = 5.00000000e-01, 5.00000000e-01, 1.00000000e+00, -1.00000000e+00
def c69 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c70 = 1.00000000e+00, 1.00000000e+00, 0.00000000e+00, 0.00000000e+00
dcl_2d s1
dcl_2d s0
rcp r0.x, c1.x
rcp r0.y, c1.x
mul r0.xy, v0.xyxx, r0.xyxx
frc r1.xyzw, r0.xyxx
add r0.xy, r0.xyxx, -r1.xyzw
add r0.zw, r0.xxxy, c68.xxxy
mul r0.zw, r0.xxzw, c1.x
mov r1.xyzw, c69.xyzw
mov r1.xy, r0.zwzz
texldl r1.xyzw, r1.xyzw, s0
add r0.xy, r0.xyxx, c0.xyxx
mov r0.z, c68.z
cmp r2.xy, r0.xyxx, c69.xyxx, c70.xyxx
add r2.xy, -r2.xyxx, c70.xyxx
min r0.w, r2.x, r2.y
add r2.xy, r0.xyxx, -c0.w
cmp r2.xy, r2.xyxx, c69.xyxx, c70.xyxx
min r2.x, r2.x, r2.y
min r0.w, r0.w, r2.x
if_ne r0.w, -r0.w
    add r0.xy, r0.xyxx, c68.xyxx
    mul r0.xy, r0.xyxx, c1.y
    mov r2.xyzw, c69.xyzw
    mov r2.xy, r0.xyxx
    texldl r2.xyzw, r2.xyzw, s1
    add r0.x, r2.x, c68.w
    cmp r0.x, r0.x, c69.x, c68.z
    add r0.y, r2.x, c0.z
    add r0.w, -r0.y, c68.z
    cmp r0.w, r0.w, c69.x, c68.z
    add r0.w, -r0.w, c68.z
    max r0.y, r0.y, c69.x
    cmp r0.y, -r0.w, c68.z, r0.y
    cmp r0.x, -r0.x, c68.z, r0.y
    mov r0.z, r0.x
else
endif
mov r0.x, r0.z
min r0.x, r1.x, r0.x
mov oC0.xyzw, r0.x
