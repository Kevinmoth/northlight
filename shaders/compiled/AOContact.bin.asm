ps_3_0
dcl_texcoord0 v0
def c5 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c6 = -9.99989986e-01, 1.00000000e+00, -5.00000000e-01, 9.99999975e-06
def c7 = 1.20000001e-02, 6.99999975e-04, 3.00000003e-03, 2.00000000e+00
def c8 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 1.00000000e+00
def c9 = -1.00000000e+00, 9.99999982e-15, 5.00000007e-02, 5.00000000e-01
def c10 = 0.00000000e+00, 0.00000000e+00, -1.00000000e+00, 9.99999975e-05
def c11 = 2.00000000e+00, 2.00000000e+00, 5.00000007e-02, 5.00000007e-02
def c12 = 5.29829178e+01, 3.74136009e+01, 6.71105608e-02, 5.83714992e-03
def c13 = -1.00000000e+00, -1.00000000e+00, -8.00000000e+00, -2.00000000e+00
def c14 = 3.23399991e-01, 1.33900002e-01, -2.48799995e-01, 6.00499988e-01
def c15 = -8.31499994e-01, -3.44399989e-01, -3.00000000e+00, -4.00000000e+00
def c16 = 3.82699996e-01, -9.23900008e-01, 8.31499994e-01, -3.44399989e-01
def c17 = -5.00000000e+00, 2.48799995e-01, 6.00499988e-01, -6.00000000e+00
def c18 = -3.23399991e-01, 1.33900002e-01, -7.00000000e+00, 9.99999996e-13
def c19 = -3.82699996e-01, -9.23900008e-01, -7.99999982e-02, 1.08695650e+00
def c20 = 1.00000001e-10, 2.50000000e-01, 0.00000000e+00, 0.00000000e+00
defi i0 = 255, 0, 0, 0
dcl_2d s1
dcl_2d s3
mov r0.xyzw, c5.xyzw
mov r0.xy, v0.xyxx
texldl r0.xyzw, r0.xyzw, s1
add r0.x, r0.x, -c3.x
mul r0.x, r0.x, c3.y
mov_sat r0.x, r0.x
add r0.y, r0.x, c6.x
cmp r0.y, r0.y, c5.x, c6.y
add r0.y, -r0.y, c6.y
add r0.z, c4.x, c6.z
cmp r0.z, r0.z, c5.x, c6.y
mov r0.w, r1.x
cmp r0.w, -r0.z, r0.w, c5.x
mov r1.x, r0.w
add r0.z, -r0.z, c6.y
if_ne r0.z, -r0.z
    mov r2.xyzw, c5.xyzw
    mov r2.xy, v0.xyxx
    texldl r2.xyzw, r2.xyzw, s3
    add r0.z, c0.z, -r2.x
    cmp r0.z, r0.z, c5.x, c6.y
    mul r0.w, c0.z, c0.w
    add r1.y, c0.w, -c0.z
    mul r1.y, r0.x, r1.y
    add r1.y, c0.w, -r1.y
    max r1.y, r1.y, c6.w
    rcp r1.y, r1.y
    mul r0.w, r0.w, r1.y
    mul r1.y, r2.x, c7.y
    max r1.y, r1.y, c7.x
    add r0.w, r0.w, r1.y
    add r0.w, r0.w, -r2.x
    cmp r0.w, r0.w, c5.x, c6.y
    add r0.w, -r0.w, c6.y
    min r0.z, r0.z, r0.w
    add r0.w, -r2.y, c7.z
    cmp r0.w, r0.w, c5.x, c6.y
    min r0.z, r0.z, r0.w
    mov r1.x, r0.z
else
endif
mov r0.z, r1.x
max r0.y, r0.y, r0.z
mov r1.xyzw, r2.xyzw
cmp r1.xyzw, -r0.y, r1.xyzw, c8.xyzw
mov r2.xyzw, r1.xyzw
add r0.y, -r0.y, c6.y
if_ne r0.y, -r0.y
    mul r0.y, c0.z, c0.w
    add r0.z, c0.w, -c0.z
    mul r0.x, r0.x, r0.z
    add r0.x, c0.w, -r0.x
    max r0.x, r0.x, c6.w
    rcp r0.x, r0.x
    mul r0.x, r0.y, r0.x
    mul r0.w, v0.x, c7.w
    add r0.w, r0.w, c9.x
    mul r0.w, r0.w, r0.x
    rcp r1.x, c1.x
    mul r0.w, r0.w, r1.x
    mul r1.x, v0.y, c7.w
    add r1.x, -r1.x, c6.y
    mul r1.x, r1.x, r0.x
    rcp r1.y, c1.y
    mul r1.x, r1.x, r1.y
    mov r1.y, r0.w
    mov r1.z, r1.x
    mov r1.w, r0.x
    mov r3.xyz, r1.yzwy
    mov r4.x, c0.x
    mov r4.y, c5.x
    mov r4.z, c5.x
    mov r4.w, c0.y
    mov r5.xy, r4.xyxx
    add r5.xy, v0.xyxx, -r5.xyxx
    mov r5.zw, r4.xxxy
    add r5.zw, v0.xxxy, -r5.xxzw
    mov r6.xyzw, c5.xyzw
    mov r6.xy, r5.zwzz
    texldl r6.xyzw, r6.xyzw, s1
    add r0.w, r6.x, -c3.x
    mul r0.w, r0.w, c3.y
    mov_sat r0.w, r0.w
    mul r0.w, r0.w, r0.z
    add r0.w, c0.w, -r0.w
    max r0.w, r0.w, c6.w
    rcp r0.w, r0.w
    mul r0.w, r0.y, r0.w
    mul r1.x, r5.x, c7.w
    add r1.x, r1.x, c9.x
    mul r1.x, r1.x, r0.w
    rcp r3.w, c1.x
    mul r1.x, r1.x, r3.w
    mul r3.w, r5.y, c7.w
    add r3.w, -r3.w, c6.y
    mul r3.w, r3.w, r0.w
    rcp r5.x, c1.y
    mul r3.w, r3.w, r5.x
    mov r5.x, r1.x
    mov r5.y, r3.w
    mov r5.z, r0.w
    mov r6.xy, r4.xyxx
    add r6.xy, v0.xyxx, r6.xyxx
    add r4.xy, v0.xyxx, r4.xyxx
    mov r7.xyzw, c5.xyzw
    mov r7.xy, r4.xyxx
    texldl r7.xyzw, r7.xyzw, s1
    add r1.x, r7.x, -c3.x
    mul r1.x, r1.x, c3.y
    mov_sat r1.x, r1.x
    mul r1.x, r1.x, r0.z
    add r1.x, c0.w, -r1.x
    max r1.x, r1.x, c6.w
    rcp r1.x, r1.x
    mul r1.x, r0.y, r1.x
    mul r3.w, r6.x, c7.w
    add r3.w, r3.w, c9.x
    mul r3.w, r3.w, r1.x
    rcp r4.x, c1.x
    mul r3.w, r3.w, r4.x
    mul r4.x, r6.y, c7.w
    add r4.x, -r4.x, c6.y
    mul r4.x, r4.x, r1.x
    rcp r4.y, c1.y
    mul r4.x, r4.x, r4.y
    mov r6.x, r3.w
    mov r6.y, r4.x
    mov r6.z, r1.x
    mov r4.xy, r4.zwzz
    add r4.xy, v0.xyxx, -r4.xyxx
    mov r7.xy, r4.zwzz
    add r7.xy, v0.xyxx, -r7.xyxx
    mov r8.xyzw, c5.xyzw
    mov r8.xy, r7.xyxx
    mov r7.xyzw, r8.xyzw
    texldl r7.xyzw, r7.xyzw, s1
    add r3.w, r7.x, -c3.x
    mul r3.w, r3.w, c3.y
    mov_sat r3.w, r3.w
    mul r3.w, r3.w, r0.z
    add r3.w, c0.w, -r3.w
    max r3.w, r3.w, c6.w
    rcp r3.w, r3.w
    mul r3.w, r0.y, r3.w
    mul r5.w, r4.x, c7.w
    add r5.w, r5.w, c9.x
    mul r5.w, r5.w, r3.w
    rcp r6.w, c1.x
    mul r5.w, r5.w, r6.w
    mul r4.x, r4.y, c7.w
    add r4.x, -r4.x, c6.y
    mul r4.x, r4.x, r3.w
    rcp r4.y, c1.y
    mul r4.x, r4.x, r4.y
    mov r7.x, r5.w
    mov r7.y, r4.x
    mov r7.z, r3.w
    mov r4.xy, r4.zwzz
    add r4.xy, v0.xyxx, r4.xyxx
    add r4.zw, v0.xxxy, r4.xxzw
    mov r8.xyzw, c5.xyzw
    mov r8.xy, r4.zwzz
    texldl r8.xyzw, r8.xyzw, s1
    add r4.z, r8.x, -c3.x
    mul r4.z, r4.z, c3.y
    mov_sat r4.z, r4.z
    mul r4.z, r4.z, r0.z
    add r4.z, c0.w, -r4.z
    max r4.z, r4.z, c6.w
    rcp r4.z, r4.z
    mul r4.z, r0.y, r4.z
    mul r4.w, r4.x, c7.w
    add r4.w, r4.w, c9.x
    mul r4.w, r4.w, r4.z
    rcp r5.w, c1.x
    mul r4.w, r4.w, r5.w
    mul r4.x, r4.y, c7.w
    add r4.x, -r4.x, c6.y
    mul r4.x, r4.x, r4.z
    rcp r4.y, c1.y
    mul r4.x, r4.x, r4.y
    mov r8.x, r4.w
    mov r8.y, r4.x
    mov r8.z, r4.z
    add r1.x, r1.x, -r0.x
    abs r1.x, r1.x
    add r0.w, r0.x, -r0.w
    abs r0.w, r0.w
    add r0.w, r1.x, -r0.w
    cmp r0.w, r0.w, c5.x, c6.y
    mov r4.xyw, r6.xyxz
    add r4.xyw, r4.xyxw, -r3.xyxz
    add r5.xyz, r3.xyzx, -r5.xyzx
    cmp r4.xyw, -r0.w, r5.xyxz, r4.xyxw
    add r0.w, r4.z, -r0.x
    abs r0.w, r0.w
    add r1.x, r0.x, -r3.w
    abs r1.x, r1.x
    add r0.w, r0.w, -r1.x
    cmp r0.w, r0.w, c5.x, c6.y
    mov r5.xyz, r8.xyzx
    add r5.xyz, r5.xyzx, -r3.xyzx
    mov r6.xyz, r7.xyzx
    add r3.xyz, r3.xyzx, -r6.xyzx
    cmp r3.xyz, -r0.w, r3.xyzx, r5.xyzx
    mul r5.xyz, r4.wxyw, r3.yzxy
    mul r3.xyz, r4.ywxy, r3.zxyz
    add r3.xyz, r3.xyzx, -r5.xyzx
    dp3 r0.w, r3.xyzx, r3.xyzx
    add r1.x, -r0.w, c9.y
    cmp r1.x, r1.x, c5.x, c6.y
    max r0.w, r0.w, c9.y
    rsq r0.w, r0.w
    mov r4.xyz, r0.w
    mul r3.xyz, r3.xyzx, r4.xyzx
    cmp r3.xyz, -r1.x, c10.xyzx, r3.xyzx
    cmp r0.w, -r3.z, c5.x, c6.y
    cmp r3.xyz, -r0.w, r3.xyzx, -r3.xyzx
    max r0.w, c2.y, c9.z
    mul r1.x, r0.w, c9.w
    mul r4.xy, r1.x, c1.xyxx
    max r0.x, r0.x, c0.z
    rcp r4.z, r0.x
    rcp r4.w, r0.x
    mul r4.xy, r4.xyxx, r4.zwzz
    mul r4.zw, c0.xxxy, c11.xxxy
    max r4.xy, r4.xyxx, r4.zwzz
    min r4.xy, r4.xyxx, c11.zwzz
    rcp r4.z, c0.x
    rcp r4.w, c0.y
    mul r4.zw, v0.xxxy, r4.xxzw
    dp2add r0.x, r4.zwzz, c12.zwzz, c5.x
    frc r0.x, r0.x
    mul r4.zw, r0.x, c12.xxxy
    frc r4.zw, r4.xxzw
    mul r4.zw, r4.xxzw, c11.xxxy
    add r4.zw, r4.xxzw, c13.xxxy
    dp2add r0.x, r4.zwzz, r4.zwzz, c5.x
    max r0.x, r0.x, c10.w
    rsq r0.x, r0.x
    mov r5.xy, r0.x
    mul r4.zw, r4.xxzw, r5.xxxy
    mul r5.xy, r4.zwzz, r4.xyxx
    mov r5.z, -r4.w
    mov r5.w, r4.z
    mov r4.zw, r5.xxzw
    mul r4.xy, r4.zwzz, r4.xyxx
    mov r0.x, c5.x
    mov r1.x, c5.x
    rep i0.xyzw
        mov r7.w, r1.x
        add r7.w, r7.w, c13.z
        cmp r7.w, r7.w, c5.x, c6.y
        add r7.w, -r7.w, c6.y
        if_ne r7.w, -r7.w
            break
        else
        endif
        mov r7.w, r1.x
        abs r9.x, r7.w
        add r9.x, -r9.x, -r9.x
        cmp r9.x, r9.x, c5.x, c6.y
        add r9.x, -r9.x, c6.y
        cmp r9.xy, -r9.x, c5.xyxx, c14.xyxx
        add r9.z, r7.w, c9.x
        abs r9.z, r9.z
        add r9.z, -r9.z, -r9.z
        cmp r9.z, r9.z, c5.x, c6.y
        add r9.z, -r9.z, c6.y
        cmp r9.xy, -r9.z, r9.xyxx, c14.zwzz
        add r9.z, r7.w, c13.w
        abs r9.z, r9.z
        add r9.z, -r9.z, -r9.z
        cmp r9.z, r9.z, c5.x, c6.y
        add r9.z, -r9.z, c6.y
        cmp r9.xy, -r9.z, r9.xyxx, c15.xyxx
        add r9.z, r7.w, c15.z
        abs r9.z, r9.z
        add r9.z, -r9.z, -r9.z
        cmp r9.z, r9.z, c5.x, c6.y
        add r9.z, -r9.z, c6.y
        cmp r9.xy, -r9.z, r9.xyxx, c16.xyxx
        add r9.z, r7.w, c15.w
        abs r9.z, r9.z
        add r9.z, -r9.z, -r9.z
        cmp r9.z, r9.z, c5.x, c6.y
        add r9.z, -r9.z, c6.y
        cmp r9.xy, -r9.z, r9.xyxx, c16.zwzz
        add r9.z, r7.w, c17.x
        abs r9.z, r9.z
        add r9.z, -r9.z, -r9.z
        cmp r9.z, r9.z, c5.x, c6.y
        add r9.z, -r9.z, c6.y
        cmp r9.xy, -r9.z, r9.xyxx, c17.yzyy
        add r9.z, r7.w, c17.w
        abs r9.z, r9.z
        add r9.z, -r9.z, -r9.z
        cmp r9.z, r9.z, c5.x, c6.y
        add r9.z, -r9.z, c6.y
        cmp r9.xy, -r9.z, r9.xyxx, c18.xyxx
        add r7.w, r7.w, c18.z
        abs r7.w, r7.w
        add r7.w, -r7.w, -r7.w
        cmp r7.w, r7.w, c5.x, c6.y
        add r7.w, -r7.w, c6.y
        cmp r9.xy, -r7.w, r9.xyxx, c19.xyxx
        mul r9.xy, r9.x, r5.xyxx
        add r9.xy, v0.xyxx, r9.xyxx
        mov r7.w, r1.x
        abs r9.z, r7.w
        add r9.z, -r9.z, -r9.z
        cmp r9.z, r9.z, c5.x, c6.y
        add r9.z, -r9.z, c6.y
        cmp r9.zw, -r9.z, c5.xxxy, c14.xxxy
        add r10.x, r7.w, c9.x
        abs r10.x, r10.x
        add r10.x, -r10.x, -r10.x
        cmp r10.x, r10.x, c5.x, c6.y
        add r10.x, -r10.x, c6.y
        cmp r9.zw, -r10.x, r9.xxzw, c14.xxzw
        add r10.x, r7.w, c13.w
        abs r10.x, r10.x
        add r10.x, -r10.x, -r10.x
        cmp r10.x, r10.x, c5.x, c6.y
        add r10.x, -r10.x, c6.y
        cmp r9.zw, -r10.x, r9.xxzw, c15.xxxy
        add r10.x, r7.w, c15.z
        abs r10.x, r10.x
        add r10.x, -r10.x, -r10.x
        cmp r10.x, r10.x, c5.x, c6.y
        add r10.x, -r10.x, c6.y
        cmp r9.zw, -r10.x, r9.xxzw, c16.xxxy
        add r10.x, r7.w, c15.w
        abs r10.x, r10.x
        add r10.x, -r10.x, -r10.x
        cmp r10.x, r10.x, c5.x, c6.y
        add r10.x, -r10.x, c6.y
        cmp r9.zw, -r10.x, r9.xxzw, c16.xxzw
        add r10.x, r7.w, c17.x
        abs r10.x, r10.x
        add r10.x, -r10.x, -r10.x
        cmp r10.x, r10.x, c5.x, c6.y
        add r10.x, -r10.x, c6.y
        cmp r9.zw, -r10.x, r9.xxzw, c17.xxyz
        add r10.x, r7.w, c17.w
        abs r10.x, r10.x
        add r10.x, -r10.x, -r10.x
        cmp r10.x, r10.x, c5.x, c6.y
        add r10.x, -r10.x, c6.y
        cmp r9.zw, -r10.x, r9.xxzw, c18.xxxy
        add r7.w, r7.w, c18.z
        abs r7.w, r7.w
        add r7.w, -r7.w, -r7.w
        cmp r7.w, r7.w, c5.x, c6.y
        add r7.w, -r7.w, c6.y
        cmp r9.zw, -r7.w, r9.xxzw, c19.xxxy
        mul r9.zw, r9.w, r4.xxxy
        add r9.xy, r9.xyxx, r9.zwzz
        mov r8.xyzw, c5.xyzw
        mov r8.xy, r9.xyxx
        mov r10.xyzw, r8.xyzw
        texldl r10.xyzw, r10.xyzw, s1
        add r7.w, r10.x, -c3.x
        mul r7.w, r7.w, c3.y
        mov_sat r7.w, r7.w
        mul r9.z, r7.w, r0.z
        add r9.z, c0.w, -r9.z
        max r9.z, r9.z, c6.w
        rcp r6.w, r9.z
        mul r9.z, r0.y, r6.w
        mul r9.w, r9.x, c7.w
        add r9.w, r9.w, c9.x
        mul r9.w, r9.w, r9.z
        rcp r4.z, c1.x
        mul r9.w, r9.w, r4.z
        mul r10.x, r9.y, c7.w
        add r10.x, -r10.x, c6.y
        mul r10.x, r10.x, r9.z
        rcp r4.w, c1.y
        mul r10.x, r10.x, r4.w
        mov r7.x, r9.w
        mov r7.y, r10.x
        mov r7.z, r9.z
        mov r10.xyz, r7.xyzx
        mov r11.xyz, r1.yzwy
        add r10.xyz, r10.xyzx, -r11.xyzx
        dp3 r9.z, r10.xyzx, r10.xyzx
        max r9.w, r9.z, c18.w
        rsq r5.w, r9.w
        rcp r5.z, r5.w
        mov r11.xyz, r5.z
        rcp r6.x, r11.x
        rcp r6.y, r11.y
        rcp r6.z, r11.z
        mul r10.xyz, r10.xyzx, r6.xyzx
        dp3 r9.w, r3.xyzx, r10.xyzx
        add r9.w, r9.w, c19.z
        mul r9.w, r9.w, c19.w
        mov_sat r9.w, r9.w
        rcp r3.w, r0.w
        mul r10.x, r5.z, r3.w
        add r10.x, -r10.x, c6.y
        mov_sat r10.x, r10.x
        mul r10.x, r10.x, r10.x
        add r7.w, r7.w, c6.x
        cmp r7.w, r7.w, c5.x, c6.y
        add r9.z, -r9.z, c20.x
        cmp r9.z, r9.z, c5.x, c6.y
        min r7.w, r7.w, r9.z
        cmp r9.z, -r9.x, c5.x, c6.y
        min r7.w, r7.w, r9.z
        cmp r9.z, -r9.y, c5.x, c6.y
        min r7.w, r7.w, r9.z
        add r9.z, r9.x, c9.x
        cmp r9.z, r9.z, c5.x, c6.y
        min r7.w, r7.w, r9.z
        add r9.x, r9.y, c9.x
        cmp r9.x, r9.x, c5.x, c6.y
        min r7.w, r7.w, r9.x
        cmp r7.w, -r7.w, c5.x, c6.y
        mul r9.x, r9.w, r10.x
        mul r7.w, r9.x, r7.w
        mov r9.x, r0.x
        add r7.w, r9.x, r7.w
        mov r0.x, r7.w
        mov r7.w, r1.x
        add r7.w, r7.w, c6.y
        mov r1.x, r7.w
    endrep
    mul r0.x, c1.z, r0.x
    mul r0.x, r0.x, c20.y
    add r0.x, -r0.x, c6.y
    mov_sat r0.x, r0.x
    mov r1.x, c5.x
    mov r1.y, c5.x
    mov r1.z, c5.x
    mov r1.w, r0.x
    mov r0.xyzw, r1.xyzw
    mov r2.xyzw, r0.xyzw
else
endif
mov oC0.xyzw, r2.xyzw
