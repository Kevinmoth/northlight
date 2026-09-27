ps_3_0
dcl_texcoord0 v0
def c68 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c69 = 1.00000000e+00, 1.00000005e-03, -1.00000000e+00, -1.00000000e+00
def c70 = 5.00000000e-01, 5.00000000e-01, -9.99989986e-01, -5.00000000e-01
def c71 = 9.99999975e-06, 1.20000001e-02, 6.99999975e-04, 3.00000003e-03
def c72 = 2.00000000e+00, -2.00000000e+00, -1.00000000e+00, 1.00000000e+00
def c73 = 2.49999994e-03, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
dcl_2d s12
dcl_2d s1
dcl_2d s14
dcl_2d s11
mov r0.xyzw, c68.xyzw
mov r0.xy, v0.xyxx
texldl r0.xyzw, r0.xyzw, s14
mov r1.xyzw, c68.xyzw
mov r1.xy, v0.xyxx
texldl r1.xyzw, r1.xyzw, s12
add r2.x, -c20.z, c69.x
add r1.x, r1.w, -r2.x
max r1.y, c20.z, c69.y
rcp r1.y, r1.y
mul r1.x, r1.x, r1.y
mov_sat r1.x, r1.x
mul r1.yz, v0.xxyx, c33.xxyx
frc r2.xyzw, r1.yzyy
add r1.yz, r1.xyzx, -r2.xxyx
add r2.xy, c33.xyxx, c69.zwzz
max r1.yz, r1.xyzx, c68.xxyx
min r1.yz, r1.xyzx, r2.xxyx
add r1.yz, r1.xyzx, c70.xxyx
mul r1.yz, r1.xyzx, c0.xxyx
mov r2.xyzw, c68.xyzw
mov r2.xy, r1.yzyy
texldl r2.xyzw, r2.xyzw, s1
add r1.w, r2.x, -c1.w
mul r1.w, r1.w, c2.x
mov_sat r1.w, r1.w
add r2.x, r1.w, c70.z
cmp r2.x, r2.x, c68.x, c69.x
add r2.x, -r2.x, c69.x
add r2.y, c30.x, c70.w
cmp r2.y, r2.y, c68.x, c69.x
mov r2.z, r2.w
cmp r2.z, -r2.y, r2.z, c68.x
mov r2.w, r2.z
add r2.y, -r2.y, c69.x
if_ne r2.y, -r2.y
    mov r3.xyzw, c68.xyzw
    mov r3.xy, r1.yzyy
    texldl r3.xyzw, r3.xyzw, s11
    add r2.y, c0.z, -r3.x
    cmp r2.y, r2.y, c68.x, c69.x
    mul r2.z, c0.z, c0.w
    add r4.x, c0.w, -c0.z
    mul r4.x, r1.w, r4.x
    add r4.x, c0.w, -r4.x
    max r4.x, r4.x, c71.x
    rcp r4.x, r4.x
    mul r2.z, r2.z, r4.x
    mul r4.x, r3.x, c71.z
    max r4.x, r4.x, c71.y
    add r2.z, r2.z, r4.x
    add r2.z, r2.z, -r3.x
    cmp r2.z, r2.z, c68.x, c69.x
    add r2.z, -r2.z, c69.x
    min r2.y, r2.y, r2.z
    add r2.z, -r3.y, c71.w
    cmp r2.z, r2.z, c68.x, c69.x
    min r2.y, r2.y, r2.z
    cmp r2.y, -r2.y, c68.x, r3.x
    mov r2.w, r2.y
else
endif
mov r2.y, r2.w
cmp r2.y, -r2.y, c68.x, c69.x
max r2.x, r2.x, r2.y
mov r3.xyzw, r4.xyzw
cmp r3.xyzw, -r2.x, r3.xyzw, c68.xyzw
mov r4.xyzw, r3.xyzw
add r2.x, -r2.x, c69.x
if_ne r2.x, -r2.x
    mul r2.x, c0.z, c0.w
    add r2.y, c0.w, -c0.z
    mul r1.w, r1.w, r2.y
    add r1.w, c0.w, -r1.w
    max r1.w, r1.w, c71.x
    rcp r1.w, r1.w
    mul r1.w, r2.x, r1.w
    mul r2.xy, c0.xyxx, c70.xyxx
    add r1.yz, r1.xyzx, -r2.xxyx
    mul r1.yz, r1.xyzx, c72.xxyx
    add r1.yz, r1.xyzx, c72.xzwx
    rcp r2.x, c1.x
    rcp r2.y, c1.y
    mul r1.yz, r1.xyzx, r2.xxyx
    mov r2.xy, r1.yzyy
    mov r2.z, c1.z
    mul r1.yzw, r2.xxyz, r1.w
    mul r2.xyz, r1.y, c3.xyzx
    mul r3.xyz, r1.z, c4.xyzx
    add r2.xyz, r2.xyzx, r3.xyzx
    mul r1.yzw, r1.w, c5.xxyz
    add r1.yzw, r2.xxyz, r1.xyzw
    add r1.yzw, r1.xyzw, c6.xxyz
    add r2.xyz, c36.xyzx, -r1.yzwy
    dp3 r2.w, r2.xyzx, r2.xyzx
    max r2.w, r2.w, c73.x
    rsq r2.w, r2.w
    rcp r3.x, r2.w
    mov r3.x, -r3.x
    add r3.x, c36.w, r3.x
    mul r3.x, r3.x, c44.w
    mov_sat r3.x, r3.x
    dp3 r2.x, r0.xyzx, r2.xyzx
    mul r2.x, r2.x, r2.w
    mov_sat r2.x, r2.x
    mul r2.y, r3.x, r3.x
    mul r2.x, r2.y, r2.x
    mul r2.xyz, c44.xyzx, r2.x
    add r3.xyz, c37.xyzx, -r1.yzwy
    dp3 r2.w, r3.xyzx, r3.xyzx
    max r2.w, r2.w, c73.x
    rsq r2.w, r2.w
    rcp r3.w, r2.w
    mov r3.w, -r3.w
    add r3.w, c37.w, r3.w
    mul r3.w, r3.w, c45.w
    mov_sat r3.w, r3.w
    dp3 r3.x, r0.xyzx, r3.xyzx
    mul r2.w, r3.x, r2.w
    mov_sat r2.w, r2.w
    mul r3.x, r3.w, r3.w
    mul r2.w, r3.x, r2.w
    mul r3.xyz, c45.xyzx, r2.w
    add r2.xyz, r2.xyzx, r3.xyzx
    add r3.xyz, c38.xyzx, -r1.yzwy
    dp3 r2.w, r3.xyzx, r3.xyzx
    max r2.w, r2.w, c73.x
    rsq r2.w, r2.w
    rcp r3.w, r2.w
    mov r3.w, -r3.w
    add r3.w, c38.w, r3.w
    mul r3.w, r3.w, c46.w
    mov_sat r3.w, r3.w
    dp3 r3.x, r0.xyzx, r3.xyzx
    mul r2.w, r3.x, r2.w
    mov_sat r2.w, r2.w
    mul r3.x, r3.w, r3.w
    mul r2.w, r3.x, r2.w
    mul r3.xyz, c46.xyzx, r2.w
    add r2.xyz, r2.xyzx, r3.xyzx
    add r3.xyz, c39.xyzx, -r1.yzwy
    dp3 r2.w, r3.xyzx, r3.xyzx
    max r2.w, r2.w, c73.x
    rsq r2.w, r2.w
    rcp r3.w, r2.w
    mov r3.w, -r3.w
    add r3.w, c39.w, r3.w
    mul r3.w, r3.w, c47.w
    mov_sat r3.w, r3.w
    dp3 r3.x, r0.xyzx, r3.xyzx
    mul r2.w, r3.x, r2.w
    mov_sat r2.w, r2.w
    mul r3.x, r3.w, r3.w
    mul r2.w, r3.x, r2.w
    mul r3.xyz, c47.xyzx, r2.w
    add r2.xyz, r2.xyzx, r3.xyzx
    add r3.xyz, c40.xyzx, -r1.yzwy
    dp3 r2.w, r3.xyzx, r3.xyzx
    max r2.w, r2.w, c73.x
    rsq r2.w, r2.w
    rcp r3.w, r2.w
    mov r3.w, -r3.w
    add r3.w, c40.w, r3.w
    mul r3.w, r3.w, c48.w
    mov_sat r3.w, r3.w
    dp3 r3.x, r0.xyzx, r3.xyzx
    mul r2.w, r3.x, r2.w
    mov_sat r2.w, r2.w
    mul r3.x, r3.w, r3.w
    mul r2.w, r3.x, r2.w
    mul r3.xyz, c48.xyzx, r2.w
    add r2.xyz, r2.xyzx, r3.xyzx
    add r3.xyz, c41.xyzx, -r1.yzwy
    dp3 r2.w, r3.xyzx, r3.xyzx
    max r2.w, r2.w, c73.x
    rsq r2.w, r2.w
    rcp r3.w, r2.w
    mov r3.w, -r3.w
    add r3.w, c41.w, r3.w
    mul r3.w, r3.w, c49.w
    mov_sat r3.w, r3.w
    dp3 r3.x, r0.xyzx, r3.xyzx
    mul r2.w, r3.x, r2.w
    mov_sat r2.w, r2.w
    mul r3.x, r3.w, r3.w
    mul r2.w, r3.x, r2.w
    mul r3.xyz, c49.xyzx, r2.w
    add r2.xyz, r2.xyzx, r3.xyzx
    add r3.xyz, c42.xyzx, -r1.yzwy
    dp3 r2.w, r3.xyzx, r3.xyzx
    max r2.w, r2.w, c73.x
    rsq r2.w, r2.w
    rcp r3.w, r2.w
    mov r3.w, -r3.w
    add r3.w, c42.w, r3.w
    mul r3.w, r3.w, c50.w
    mov_sat r3.w, r3.w
    dp3 r3.x, r0.xyzx, r3.xyzx
    mul r2.w, r3.x, r2.w
    mov_sat r2.w, r2.w
    mul r3.x, r3.w, r3.w
    mul r2.w, r3.x, r2.w
    mul r3.xyz, c50.xyzx, r2.w
    add r2.xyz, r2.xyzx, r3.xyzx
    add r1.yzw, c43.xxyz, -r1.xyzw
    dp3 r2.w, r1.yzwy, r1.yzwy
    max r2.w, r2.w, c73.x
    rsq r2.w, r2.w
    rcp r3.x, r2.w
    mov r3.x, -r3.x
    add r3.x, c43.w, r3.x
    mul r3.x, r3.x, c51.w
    mov_sat r3.x, r3.x
    dp3 r0.x, r0.xyzx, r1.yzwy
    mul r0.x, r0.x, r2.w
    mov_sat r0.x, r0.x
    mul r0.y, r3.x, r3.x
    mul r0.x, r0.y, r0.x
    mul r0.xyz, c51.xyzx, r0.x
    add r0.xyz, r2.xyzx, r0.xyzx
    mul r0.w, c52.z, r1.x
    add r0.w, -r0.w, c69.x
    mul r0.w, c52.y, r0.w
    mul r0.xyz, r0.xyzx, r0.w
    mov r0.w, c68.x
    mov r4.xyzw, r0.xyzw
else
endif
mov oC0.xyzw, r4.xyzw
