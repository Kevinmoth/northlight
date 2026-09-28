ps_3_0
dcl_texcoord0 v0
def c5 = 4.00000000e+00, 0.00000000e+00, 0.00000000e+00, 4.00000000e+00
def c6 = 2.12599993e-01, 7.15200007e-01, 7.22000003e-02, 3.57142854e+00
def c7 = -2.57142878e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c8 = 4.00000000e+00, 4.00000000e+00, 4.00000000e+00, 1.25000000e-01
def c9 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c10 = -9.99989986e-01, 1.00000000e+00, -5.00000000e-01, 9.99999975e-06
def c11 = 1.20000001e-02, 6.99999975e-04, 3.00000003e-03, 2.00000000e+00
def c12 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 1.00000000e+00
def c13 = -1.00000000e+00, 9.99999982e-15, 5.00000007e-02, 5.00000000e-01
def c14 = 0.00000000e+00, 0.00000000e+00, -1.00000000e+00, 9.99999975e-05
def c15 = 2.00000000e+00, 2.00000000e+00, 5.00000007e-02, 5.00000007e-02
def c16 = 5.29829178e+01, 3.74136009e+01, 6.71105608e-02, 5.83714992e-03
def c17 = -1.00000000e+00, -1.00000000e+00, -8.00000000e+00, -2.00000000e+00
def c18 = 3.23399991e-01, 1.33900002e-01, -2.48799995e-01, 6.00499988e-01
def c19 = -8.31499994e-01, -3.44399989e-01, -3.00000000e+00, -4.00000000e+00
def c20 = 3.82699996e-01, -9.23900008e-01, 8.31499994e-01, -3.44399989e-01
def c21 = -5.00000000e+00, 2.48799995e-01, 6.00499988e-01, -6.00000000e+00
def c22 = -3.23399991e-01, 1.33900002e-01, -7.00000000e+00, 9.99999996e-13
def c23 = -3.82699996e-01, -9.23900008e-01, -7.99999982e-02, 1.08695650e+00
def c24 = 1.00000001e-10, 2.50000000e-01, 0.00000000e+00, 0.00000000e+00
defi i0 = 255, 0, 0, 0
dcl_2d s1
dcl_2d s0
dcl_2d s3
mul r0.xyzw, c0.xyxy, c5.xyzw
texld r1.xyzw, v0.xyxx, s0
dp3 r2.x, r1.xyzx, c6.xyzx
mad r2.x, r2.x, c6.w, c7.x
mov_sat r2.x, r2.x
mad r1.xyz, r1.xyzx, r2.x, c7.yzwy
mul r1.xyz, r1.xyzx, c8.xyzx
add r2.xy, v0.xyxx, r0.xyxx
texld r2.xyzw, r2.xyxx, s0
dp3 r1.w, r2.xyzx, c6.xyzx
mad r1.w, r1.w, c6.w, c7.x
mov_sat r1.w, r1.w
mad r1.xyz, r2.xyzx, r1.w, r1.xyzx
add r2.xy, v0.xyxx, -r0.xyxx
texld r2.xyzw, r2.xyxx, s0
dp3 r1.w, r2.xyzx, c6.xyzx
mad r1.w, r1.w, c6.w, c7.x
mov_sat r1.w, r1.w
mad r1.xyz, r2.xyzx, r1.w, r1.xyzx
add r2.xy, v0.xyxx, r0.zwzz
texld r2.xyzw, r2.xyxx, s0
dp3 r1.w, r2.xyzx, c6.xyzx
mad r1.w, r1.w, c6.w, c7.x
mov_sat r1.w, r1.w
mad r1.xyz, r2.xyzx, r1.w, r1.xyzx
add r0.xy, v0.xyxx, -r0.zwzz
texld r0.xyzw, r0.xyxx, s0
dp3 r1.w, r0.xyzx, c6.xyzx
mad r1.w, r1.w, c6.w, c7.x
mov_sat r1.w, r1.w
mad r0.xyz, r0.xyzx, r1.w, r1.xyzx
mul r0.w, c2.x, c8.w
mul r0.xyz, r0.xyzx, r0.w
mov r1.xyzw, c9.xyzw
mov r1.xy, v0.xyxx
texldl r1.xyzw, r1.xyzw, s1
add r0.w, r1.x, -c3.x
mul r0.w, r0.w, c3.y
mov_sat r0.w, r0.w
add r1.x, r0.w, c10.x
cmp r1.x, r1.x, c5.y, c10.y
add r1.x, -r1.x, c10.y
add r1.y, c4.x, c10.z
cmp r1.y, r1.y, c5.y, c10.y
mov r1.z, r1.w
cmp r1.z, -r1.y, r1.z, c5.y
mov r1.w, r1.z
add r1.y, -r1.y, c10.y
if_ne r1.y, -r1.y
    mov r2.xyzw, c9.xyzw
    mov r2.xy, v0.xyxx
    texldl r2.xyzw, r2.xyzw, s3
    add r1.y, c0.z, -r2.x
    cmp r1.y, r1.y, c5.y, c10.y
    mul r1.z, c0.z, c0.w
    add r3.x, c0.w, -c0.z
    mul r3.x, r0.w, r3.x
    add r3.x, c0.w, -r3.x
    max r3.x, r3.x, c10.w
    rcp r3.x, r3.x
    mul r1.z, r1.z, r3.x
    mul r3.x, r2.x, c11.y
    max r3.x, r3.x, c11.x
    add r1.z, r1.z, r3.x
    add r1.z, r1.z, -r2.x
    cmp r1.z, r1.z, c5.y, c10.y
    add r1.z, -r1.z, c10.y
    min r1.y, r1.y, r1.z
    add r1.z, -r2.y, c11.z
    cmp r1.z, r1.z, c5.y, c10.y
    min r1.y, r1.y, r1.z
    mov r1.w, r1.y
else
endif
mov r1.y, r1.w
max r1.x, r1.x, r1.y
mov r2.xyzw, r3.xyzw
cmp r2.xyzw, -r1.x, r2.xyzw, c12.xyzw
mov r3.xyzw, r2.xyzw
add r1.x, -r1.x, c10.y
if_ne r1.x, -r1.x
    mul r1.x, c0.z, c0.w
    add r1.y, c0.w, -c0.z
    mul r0.w, r0.w, r1.y
    add r0.w, c0.w, -r0.w
    max r0.w, r0.w, c10.w
    rcp r0.w, r0.w
    mul r0.w, r1.x, r0.w
    mul r1.z, v0.x, c11.w
    add r1.z, r1.z, c13.x
    mul r1.z, r1.z, r0.w
    rcp r1.w, c1.x
    mul r1.z, r1.z, r1.w
    mul r1.w, v0.y, c11.w
    add r1.w, -r1.w, c10.y
    mul r1.w, r1.w, r0.w
    rcp r2.x, c1.y
    mul r1.w, r1.w, r2.x
    mov r2.x, r1.z
    mov r2.y, r1.w
    mov r2.z, r0.w
    mov r4.xyz, r2.xyzx
    mov r1.z, c0.x
    mov r1.w, c5.y
    mov r5.x, c5.y
    mov r5.y, c0.y
    mov r5.zw, r1.xxzw
    add r5.zw, v0.xxxy, -r5.xxzw
    mov r6.xy, r1.zwzz
    add r6.xy, v0.xyxx, -r6.xyxx
    mov r7.xyzw, c9.xyzw
    mov r7.xy, r6.xyxx
    mov r6.xyzw, r7.xyzw
    texldl r6.xyzw, r6.xyzw, s1
    add r2.w, r6.x, -c3.x
    mul r2.w, r2.w, c3.y
    mov_sat r2.w, r2.w
    mul r2.w, r2.w, r1.y
    add r2.w, c0.w, -r2.w
    max r2.w, r2.w, c10.w
    rcp r2.w, r2.w
    mul r2.w, r1.x, r2.w
    mul r4.w, r5.z, c11.w
    add r4.w, r4.w, c13.x
    mul r4.w, r4.w, r2.w
    rcp r6.x, c1.x
    mul r4.w, r4.w, r6.x
    mul r5.z, r5.w, c11.w
    add r5.z, -r5.z, c10.y
    mul r5.z, r5.z, r2.w
    rcp r5.w, c1.y
    mul r5.z, r5.z, r5.w
    mov r6.x, r4.w
    mov r6.y, r5.z
    mov r6.z, r2.w
    mov r5.zw, r1.xxzw
    add r5.zw, v0.xxxy, r5.xxzw
    add r1.zw, v0.xxxy, r1.xxzw
    mov r7.xyzw, c9.xyzw
    mov r7.xy, r1.zwzz
    texldl r7.xyzw, r7.xyzw, s1
    add r1.z, r7.x, -c3.x
    mul r1.z, r1.z, c3.y
    mov_sat r1.z, r1.z
    mul r1.z, r1.z, r1.y
    add r1.z, c0.w, -r1.z
    max r1.z, r1.z, c10.w
    rcp r1.z, r1.z
    mul r1.z, r1.x, r1.z
    mul r1.w, r5.z, c11.w
    add r1.w, r1.w, c13.x
    mul r1.w, r1.w, r1.z
    rcp r4.w, c1.x
    mul r1.w, r1.w, r4.w
    mul r4.w, r5.w, c11.w
    add r4.w, -r4.w, c10.y
    mul r4.w, r4.w, r1.z
    rcp r5.z, c1.y
    mul r4.w, r4.w, r5.z
    mov r7.x, r1.w
    mov r7.y, r4.w
    mov r7.z, r1.z
    mov r5.zw, r5.xxxy
    add r5.zw, v0.xxxy, -r5.xxzw
    mov r8.xy, r5.xyxx
    add r8.xy, v0.xyxx, -r8.xyxx
    mov r9.xyzw, c9.xyzw
    mov r9.xy, r8.xyxx
    mov r8.xyzw, r9.xyzw
    texldl r8.xyzw, r8.xyzw, s1
    add r1.w, r8.x, -c3.x
    mul r1.w, r1.w, c3.y
    mov_sat r1.w, r1.w
    mul r1.w, r1.w, r1.y
    add r1.w, c0.w, -r1.w
    max r1.w, r1.w, c10.w
    rcp r1.w, r1.w
    mul r1.w, r1.x, r1.w
    mul r4.w, r5.z, c11.w
    add r4.w, r4.w, c13.x
    mul r4.w, r4.w, r1.w
    rcp r6.w, c1.x
    mul r4.w, r4.w, r6.w
    mul r5.z, r5.w, c11.w
    add r5.z, -r5.z, c10.y
    mul r5.z, r5.z, r1.w
    rcp r5.w, c1.y
    mul r5.z, r5.z, r5.w
    mov r8.x, r4.w
    mov r8.y, r5.z
    mov r8.z, r1.w
    mov r5.zw, r5.xxxy
    add r5.zw, v0.xxxy, r5.xxzw
    add r5.xy, v0.xyxx, r5.xyxx
    mov r9.xyzw, c9.xyzw
    mov r9.xy, r5.xyxx
    texldl r9.xyzw, r9.xyzw, s1
    add r4.w, r9.x, -c3.x
    mul r4.w, r4.w, c3.y
    mov_sat r4.w, r4.w
    mul r4.w, r4.w, r1.y
    add r4.w, c0.w, -r4.w
    max r4.w, r4.w, c10.w
    rcp r4.w, r4.w
    mul r4.w, r1.x, r4.w
    mul r5.x, r5.z, c11.w
    add r5.x, r5.x, c13.x
    mul r5.x, r5.x, r4.w
    rcp r5.y, c1.x
    mul r5.x, r5.x, r5.y
    mul r5.y, r5.w, c11.w
    add r5.y, -r5.y, c10.y
    mul r5.y, r5.y, r4.w
    rcp r5.z, c1.y
    mul r5.y, r5.y, r5.z
    mov r5.z, r5.y
    mov r5.w, r4.w
    add r1.z, r1.z, -r0.w
    abs r1.z, r1.z
    add r2.w, r0.w, -r2.w
    abs r2.w, r2.w
    add r1.z, r1.z, -r2.w
    cmp r1.z, r1.z, c5.y, c10.y
    add r7.xyz, r7.xyzx, -r4.xyzx
    add r6.xyz, r4.xyzx, -r6.xyzx
    cmp r6.xyz, -r1.z, r6.xyzx, r7.xyzx
    add r1.z, r4.w, -r0.w
    abs r1.z, r1.z
    add r1.w, r0.w, -r1.w
    abs r1.w, r1.w
    add r1.z, r1.z, -r1.w
    cmp r1.z, r1.z, c5.y, c10.y
    mov r5.xyz, r5.xzwx
    add r5.xyz, r5.xyzx, -r4.xyzx
    mov r7.xyz, r8.xyzx
    add r4.xyz, r4.xyzx, -r7.xyzx
    cmp r4.xyz, -r1.z, r4.xyzx, r5.xyzx
    mul r5.xyz, r6.zxyz, r4.yzxy
    mul r4.xyz, r6.yzxy, r4.zxyz
    add r4.xyz, r4.xyzx, -r5.xyzx
    dp3 r1.z, r4.xyzx, r4.xyzx
    add r1.w, -r1.z, c13.y
    cmp r1.w, r1.w, c5.y, c10.y
    max r1.z, r1.z, c13.y
    rsq r1.z, r1.z
    mov r5.xyz, r1.z
    mul r4.xyz, r4.xyzx, r5.xyzx
    cmp r4.xyz, -r1.w, c14.xyzx, r4.xyzx
    cmp r1.z, -r4.z, c5.y, c10.y
    cmp r4.xyz, -r1.z, r4.xyzx, -r4.xyzx
    max r1.z, c2.y, c13.z
    mul r1.w, r1.z, c13.w
    mul r5.xy, r1.w, c1.xyxx
    max r0.w, r0.w, c0.z
    rcp r5.z, r0.w
    rcp r5.w, r0.w
    mul r5.xy, r5.xyxx, r5.zwzz
    mul r5.zw, c0.xxxy, c15.xxxy
    max r5.xy, r5.xyxx, r5.zwzz
    min r5.xy, r5.xyxx, c15.zwzz
    rcp r5.z, c0.x
    rcp r5.w, c0.y
    mul r5.zw, v0.xxxy, r5.xxzw
    dp2add r0.w, r5.zwzz, c16.zwzz, c5.y
    frc r0.w, r0.w
    mul r5.zw, r0.w, c16.xxxy
    frc r5.zw, r5.xxzw
    mul r5.zw, r5.xxzw, c15.xxxy
    add r5.zw, r5.xxzw, c17.xxxy
    dp2add r0.w, r5.zwzz, r5.zwzz, c5.y
    max r0.w, r0.w, c14.w
    rsq r0.w, r0.w
    mov r6.xy, r0.w
    mul r5.zw, r5.xxzw, r6.xxxy
    mul r6.xy, r5.zwzz, r5.xyxx
    mov r6.z, -r5.w
    mov r6.w, r5.z
    mov r5.zw, r6.xxzw
    mul r5.xy, r5.zwzz, r5.xyxx
    mov r0.w, c5.y
    mov r1.w, c5.y
    rep i0.xyzw
        mov r8.w, r1.w
        add r8.w, r8.w, c17.z
        cmp r8.w, r8.w, c5.y, c10.y
        add r8.w, -r8.w, c10.y
        if_ne r8.w, -r8.w
            break
        else
        endif
        mov r8.w, r1.w
        abs r9.w, r8.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.xy, -r9.w, c5.yzyy, c18.xyxx
        add r9.w, r8.w, c13.x
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.xy, -r9.w, r10.xyxx, c18.zwzz
        add r9.w, r8.w, c17.w
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.xy, -r9.w, r10.xyxx, c19.xyxx
        add r9.w, r8.w, c19.z
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.xy, -r9.w, r10.xyxx, c20.xyxx
        add r9.w, r8.w, c19.w
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.xy, -r9.w, r10.xyxx, c20.zwzz
        add r9.w, r8.w, c21.x
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.xy, -r9.w, r10.xyxx, c21.yzyy
        add r9.w, r8.w, c21.w
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.xy, -r9.w, r10.xyxx, c22.xyxx
        add r8.w, r8.w, c22.z
        abs r8.w, r8.w
        add r8.w, -r8.w, -r8.w
        cmp r8.w, r8.w, c5.y, c10.y
        add r8.w, -r8.w, c10.y
        cmp r10.xy, -r8.w, r10.xyxx, c23.xyxx
        mul r10.xy, r10.x, r6.xyxx
        add r10.xy, v0.xyxx, r10.xyxx
        mov r8.w, r1.w
        abs r9.w, r8.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.zw, -r9.w, c5.xxyz, c18.xxxy
        add r9.w, r8.w, c13.x
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.zw, -r9.w, r10.xxzw, c18.xxzw
        add r9.w, r8.w, c17.w
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.zw, -r9.w, r10.xxzw, c19.xxxy
        add r9.w, r8.w, c19.z
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.zw, -r9.w, r10.xxzw, c20.xxxy
        add r9.w, r8.w, c19.w
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.zw, -r9.w, r10.xxzw, c20.xxzw
        add r9.w, r8.w, c21.x
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.zw, -r9.w, r10.xxzw, c21.xxyz
        add r9.w, r8.w, c21.w
        abs r9.w, r9.w
        add r9.w, -r9.w, -r9.w
        cmp r9.w, r9.w, c5.y, c10.y
        add r9.w, -r9.w, c10.y
        cmp r10.zw, -r9.w, r10.xxzw, c22.xxxy
        add r8.w, r8.w, c22.z
        abs r8.w, r8.w
        add r8.w, -r8.w, -r8.w
        cmp r8.w, r8.w, c5.y, c10.y
        add r8.w, -r8.w, c10.y
        cmp r10.zw, -r8.w, r10.xxzw, c23.xxxy
        mul r10.zw, r10.w, r5.xxxy
        add r10.xy, r10.xyxx, r10.zwzz
        mov r7.xyzw, c9.xyzw
        mov r7.xy, r10.xyxx
        mov r11.xyzw, r7.xyzw
        texldl r11.xyzw, r11.xyzw, s1
        add r8.w, r11.x, -c3.x
        mul r8.w, r8.w, c3.y
        mov_sat r8.w, r8.w
        mul r9.w, r8.w, r1.y
        add r9.w, c0.w, -r9.w
        max r9.w, r9.w, c10.w
        rcp r6.z, r9.w
        mul r9.w, r1.x, r6.z
        mul r10.z, r10.x, c11.w
        add r10.z, r10.z, c13.x
        mul r10.z, r10.z, r9.w
        rcp r2.w, c1.x
        mul r10.z, r10.z, r2.w
        mul r10.w, r10.y, c11.w
        add r10.w, -r10.w, c10.y
        mul r10.w, r10.w, r9.w
        rcp r6.w, c1.y
        mul r10.w, r10.w, r6.w
        mov r8.x, r10.z
        mov r8.y, r10.w
        mov r8.z, r9.w
        mov r11.xyz, r8.xyzx
        mov r12.xyz, r2.xyzx
        add r11.xyz, r11.xyzx, -r12.xyzx
        dp3 r9.w, r11.xyzx, r11.xyzx
        max r10.z, r9.w, c22.w
        rsq r4.w, r10.z
        rcp r5.z, r4.w
        mov r12.xyz, r5.z
        rcp r9.x, r12.x
        rcp r9.y, r12.y
        rcp r9.z, r12.z
        mul r11.xyz, r11.xyzx, r9.xyzx
        dp3 r10.z, r4.xyzx, r11.xyzx
        add r10.z, r10.z, c23.z
        mul r10.z, r10.z, c23.w
        mov_sat r10.z, r10.z
        rcp r5.w, r1.z
        mul r10.w, r5.z, r5.w
        add r10.w, -r10.w, c10.y
        mov_sat r10.w, r10.w
        mul r10.w, r10.w, r10.w
        add r8.w, r8.w, c10.x
        cmp r8.w, r8.w, c5.y, c10.y
        add r9.w, -r9.w, c24.x
        cmp r9.w, r9.w, c5.y, c10.y
        min r8.w, r8.w, r9.w
        cmp r9.w, -r10.x, c5.y, c10.y
        min r8.w, r8.w, r9.w
        cmp r9.w, -r10.y, c5.y, c10.y
        min r8.w, r8.w, r9.w
        add r9.w, r10.x, c13.x
        cmp r9.w, r9.w, c5.y, c10.y
        min r8.w, r8.w, r9.w
        add r9.w, r10.y, c13.x
        cmp r9.w, r9.w, c5.y, c10.y
        min r8.w, r8.w, r9.w
        cmp r8.w, -r8.w, c5.y, c10.y
        mul r9.w, r10.z, r10.w
        mul r8.w, r9.w, r8.w
        mov r9.w, r0.w
        add r8.w, r9.w, r8.w
        mov r0.w, r8.w
        mov r8.w, r1.w
        add r8.w, r8.w, c10.y
        mov r1.w, r8.w
    endrep
    mul r0.w, c1.z, r0.w
    mul r0.w, r0.w, c24.y
    add r0.w, -r0.w, c10.y
    mov_sat r0.w, r0.w
    mov r1.x, c5.y
    mov r1.y, c5.y
    mov r1.z, c5.y
    mov r1.w, r0.w
    mov r3.xyzw, r1.xyzw
else
endif
mov r1.xyzw, r3.xyzw
mov r0.w, r1.w
mov oC0.xyzw, r0.xyzw
