ps_3_0
dcl_texcoord0 v0
def c29 = 5.00000000e-01, 5.00000000e-01, -1.00000000e+00, -1.00000000e+00
def c30 = 0.00000000e+00, 0.00000000e+00, 9.99999975e-06, 1.20000001e-02
def c31 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c32 = 2.00000000e+00, -2.00000000e+00, -1.00000000e+00, 1.00000000e+00
def c33 = 6.99999975e-04, 3.00000003e-03, -5.00000000e-01, 4.00000000e+00
def c34 = 7.20000029e-01, 3.10000002e-01, 1.10000002e+00, 7.30000019e-01
def c35 = -4.09999996e-01, 8.89999986e-01, 1.73000002e+00, -1.21000004e+00
def c36 = 1.63000000e+00, 6.28318548e+00, 1.59154937e-01, -3.14159274e+00
def c37 = 5.49999997e-02, 5.49999997e-02, 3.50000001e-02, 3.50000001e-02
def c38 = 1.20000001e-02, 1.20000001e-02, 4.51999992e-01, 2.03699991e-02
def c39 = 9.79629993e-01, 5.00000000e+00, 3.84000000e+02, -9.49999988e-01
def c40 = 6.00000000e+00, 6.00000000e+00, 6.00000000e+00, -4.00000000e+00
def c41 = -1.00000005e-03, -1.00000005e-03, 1.00000000e+00, 1.00000000e+00
def c42 = 9.99000013e-01, 9.99000013e-01, -1.19999997e-04, 2.50000000e-01
def c43 = 5.12000000e+02, 4.00000000e+00, 4.00000000e+00, 4.00000000e+00
def c44 = 2.38418608e-07, 1.49999997e-02, -8.00000012e-01, -7.99999982e-02
def c45 = 1.38888884e+00, 3.00000000e+00, 5.09999990e+00, 3.70000005e+00
def c46 = 1.50000000e+00, -3.49999994e-01, 2.22222209e+00, 4.03000021e+00
defi i0 = 255, 0, 0, 0
dcl_2d s1
dcl_2d s2
dcl_2d s7
dcl_2d s6
dcl_2d s3
dcl_2d s0
dcl_2d s5
dcl_2d s4
rcp r0.x, c0.x
rcp r0.y, c0.y
add r0.xy, r0.xyxx, c29.xyxx
frc r1.xyzw, r0.xyxx
add r0.xy, r0.xyxx, -r1.xyzw
mul r0.zw, v0.xxxy, r0.xxxy
frc r1.xyzw, r0.zwzz
add r0.zw, r0.xxzw, -r1.xxxy
add r0.xy, r0.xyxx, c29.zwzz
max r0.zw, r0.xxzw, c30.xxxy
min r0.xy, r0.zwzz, r0.xyxx
add r0.xy, r0.xyxx, c29.xyxx
mul r0.xy, r0.xyxx, c0.xyxx
mov r1.xyzw, c31.xyzw
mov r1.xy, r0.xyxx
texldl r1.xyzw, r1.xyzw, s0
mov r2.xyzw, c31.xyzw
mov r2.xy, r0.xyxx
texldl r2.xyzw, r2.xyzw, s2
mov r3.xyzw, c31.xyzw
mov r3.xy, r0.xyxx
texldl r3.xyzw, r3.xyzw, s1
add r0.z, r3.x, -c1.w
mul r0.z, r0.z, c2.x
mov_sat r0.z, r0.z
mul r0.w, c0.z, c0.w
add r3.x, c0.w, -c0.z
mul r0.z, r0.z, r3.x
add r0.z, c0.w, -r0.z
max r0.z, r0.z, c30.z
rcp r0.z, r0.z
mul r0.z, r0.w, r0.z
max r0.w, r2.x, c0.z
mul r3.xy, c0.xyxx, c29.xyxx
add r3.xy, r0.xyxx, -r3.xyxx
mul r3.xy, r3.xyxx, c32.xyxx
add r3.xy, r3.xyxx, c32.zwzz
rcp r3.z, c1.x
rcp r3.w, c1.y
mul r3.zw, r3.xxxy, r3.xxzw
mov r4.xy, r3.zwzz
mov r4.z, c1.z
mov r5.xyz, r4.xyzx
mul r5.xyz, r5.xyzx, r0.w
mul r6.xyz, r5.x, c3.xyzx
mul r7.xyz, r5.y, c4.xyzx
add r6.xyz, r6.xyzx, r7.xyzx
mul r5.xyz, r5.z, c5.xyzx
add r5.xyz, r6.xyzx, r5.xyzx
add r5.xyz, r5.xyzx, c6.xyzx
dsx r6.xyz, r5.xyzx
dsy r7.xyz, r5.xyzx
dp3 r0.w, r6.xyzx, r6.xyzx
dp3 r3.z, r7.xyzx, r7.xyzx
max r0.w, r0.w, r3.z
add r3.z, c0.z, -r2.x
cmp r3.z, r3.z, c30.x, c32.w
mul r3.w, r2.x, c33.x
max r3.w, r3.w, c30.w
add r3.w, r0.z, r3.w
add r3.w, r3.w, -r2.x
cmp r3.w, r3.w, c30.x, c32.w
add r3.w, -r3.w, c32.w
min r3.z, r3.z, r3.w
add r3.w, -r2.y, c33.y
cmp r3.w, r3.w, c30.x, c32.w
min r3.z, r3.z, r3.w
cmp r3.z, -r3.z, c30.x, c32.w
add r3.z, r3.z, c33.z
cmp r3.z, r3.z, c30.x, c32.w
mov r6.xyzw, r7.xyzw
cmp r6.xyzw, -r3.z, r6.xyzw, r1.xyzw
mov r7.xyzw, r6.xyzw
add r3.z, -r3.z, c32.w
if_ne r3.z, -r3.z
    mov r6.xyzw, c31.xyzw
    mov r6.xy, r0.xyxx
    texldl r6.xyzw, r6.xyzw, s3
    mul r0.x, r2.y, c33.w
    mov_sat r0.x, r0.x
    dp2add r0.y, r5.xyxx, c34.xyxx, c30.x
    mul r3.z, c2.y, c34.z
    add r0.y, r0.y, -r3.z
    dp2add r3.z, r5.xyxx, c35.xyxx, c30.x
    mul r3.w, c2.y, c34.w
    add r3.z, r3.z, r3.w
    dp2add r3.w, r5.xyxx, c35.zwzz, c30.x
    mul r4.w, c2.y, c36.x
    add r3.w, r3.w, -r4.w
    mad r0.y, r0.y, c36.z, c29.x
    frc r0.y, r0.y
    mad r0.y, r0.y, c36.y, c36.w
    sincos r8.x, r0.y
    mov r8.xy, r8.x
    mul r8.xy, r8.xyxx, c34.xyxx
    mul r8.xy, r8.xyxx, c37.xyxx
    mad r0.y, r3.z, c36.z, c29.x
    frc r0.y, r0.y
    mad r0.y, r0.y, c36.y, c36.w
    sincos r9.x, r0.y
    mov r8.zw, r9.x
    mul r8.zw, r8.xxzw, c35.xxxy
    mul r8.zw, r8.xxzw, c37.xxzw
    add r8.xy, r8.xyxx, r8.zwzz
    mad r0.y, r3.w, c36.z, c29.x
    frc r0.y, r0.y
    mad r0.y, r0.y, c36.y, c36.w
    sincos r9.x, r0.y
    mov r3.zw, r9.x
    mul r3.zw, r3.xxzw, c35.xxzw
    mul r3.zw, r3.xxzw, c38.xxxy
    add r3.zw, r8.xxxy, r3.xxzw
    mul r0.y, r0.w, c38.z
    add r0.y, -r0.y, c32.w
    mov_sat r0.y, r0.y
    mul r3.zw, r3.xxzw, r0.y
    mov r8.xy, -r3.zwzz
    mov r8.z, c32.w
    dp3 r0.y, r8.xyzx, r8.xyzx
    rsq r0.y, r0.y
    mov r9.xyz, r0.y
    mul r8.xyz, r9.xyzx, r8.xyzx
    add r9.xyz, r5.xyzx, -c6.xyzx
    dp3 r0.y, r9.xyzx, r9.xyzx
    rsq r0.y, r0.y
    mov r10.xyz, r0.y
    mul r9.xyz, r10.xyzx, r9.xyzx
    dp3 r0.y, r8.xyzx, -r9.xyzx
    cmp r0.y, r0.y, c30.x, c32.w
    cmp r8.xyz, -r0.y, r8.xyzx, -r8.xyzx
    dp3 r0.y, r9.xyzx, r8.xyzx
    add r0.y, r0.y, r0.y
    mul r10.xyz, r8.xyzx, r0.y
    add r10.xyz, r9.xyzx, -r10.xyzx
    dp3 r0.y, -r9.xyzx, r8.xyzx
    mov_sat r0.y, r0.y
    add r0.y, -r0.y, c32.w
    log r0.y, r0.y
    mul r0.y, r0.y, c39.y
    exp r0.y, r0.y
    mul r0.y, r0.y, c39.x
    add r0.y, r0.y, c38.w
    dp3 r3.z, r10.xyzx, c7.xyzx
    mov_sat r3.z, r3.z
    log r3.z, r3.z
    mul r3.z, r3.z, c39.z
    exp r3.z, r3.z
    mov r8.xyz, r3.z
    mul r8.xyz, c8.xyzx, r8.xyzx
    mul r8.xyz, r8.xyzx, c40.xyzx
    add r3.z, c28.x, c33.z
    cmp r3.z, r3.z, c30.x, c32.w
    mov r3.w, r4.w
    cmp r3.w, -r3.z, r3.w, c32.w
    mov r4.w, r3.w
    add r5.w, -r3.z, c32.w
    if_ne r5.w, -r5.w
        mul r9.xyz, r5.x, c12.xyzx
        mul r11.xyz, r5.y, c13.xyzx
        add r9.xyz, r9.xyzx, r11.xyzx
        mul r11.xyz, r5.z, c14.xyzx
        add r9.xyz, r9.xyzx, r11.xyzx
        add r9.xyz, r9.xyzx, c15.xyzx
        mul r11.xyz, r5.x, c16.xyzx
        mul r12.xyz, r5.y, c17.xyzx
        add r11.xyz, r11.xyzx, r12.xyzx
        mul r12.xyz, r5.z, c18.xyzx
        add r11.xyz, r11.xyzx, r12.xyzx
        add r11.xyz, r11.xyzx, c19.xyzx
        abs r12.xy, r9.xyxx
        max r5.w, r12.x, r12.y
        add r5.w, r5.w, c39.w
        cmp r5.w, r5.w, c30.x, c32.w
        cmp r9.xyz, -r5.w, r11.xyzx, r9.xyzx
        mul r8.w, r9.x, c29.x
        add r8.w, r8.w, c29.x
        mul r9.w, r9.y, c29.x
        add r9.w, -r9.w, c29.x
        mov r11.x, r8.w
        mov r11.y, r9.w
        mov r11.zw, r11.xxxy
        mul r8.w, c28.z, c29.x
        add r11.zw, r11.xxzw, r8.w
        add r12.xy, r11.zwzz, c41.xyxx
        cmp r12.xy, r12.xyxx, c30.xyxx, c41.zwzz
        max r8.w, r12.x, r12.y
        add r12.xy, -r11.zwzz, c42.xyxx
        cmp r12.xy, r12.xyxx, c30.xyxx, c41.zwzz
        max r9.w, r12.x, r12.y
        max r8.w, r8.w, r9.w
        cmp r9.w, r9.z, c30.x, c32.w
        max r8.w, r8.w, r9.w
        add r9.w, -r9.z, c32.w
        cmp r9.w, r9.w, c30.x, c32.w
        max r8.w, r8.w, r9.w
        cmp r3.w, -r8.w, r3.w, c32.w
        mov r4.w, r3.w
        cmp r3.z, -r8.w, r3.z, c32.w
        add r3.z, -r3.z, c32.w
        if_ne r3.z, -r3.z
            mov r3.z, c30.x
            mov r3.w, c30.x
            rep i0.xyzw
                mov r8.w, r3.w
                add r8.w, r8.w, c40.w
                cmp r8.w, r8.w, c30.x, c32.w
                add r8.w, -r8.w, c32.w
                if_ne r8.w, -r8.w
                    break
                else
                endif
                mov r8.w, r3.w
                mul r8.w, r8.w, c29.x
                frc r8.w, r8.w
                mul r8.w, r8.w, c32.x
                add r8.w, r8.w, c33.z
                mov r9.w, r3.w
                mul r9.w, r9.w, c29.x
                frc r15.xyzw, r9.w
                add r9.w, r9.w, -r15.x
                add r9.w, r9.w, c33.z
                mov r14.x, r8.w
                mov r14.y, r9.w
                mov r14.zw, r14.xxxy
                mul r14.zw, r14.xxzw, c28.z
                add r14.zw, r11.xxzw, r14.xxzw
                mov r12.xyzw, c31.xyzw
                mov r12.xy, r14.zwzz
                mov r15.xyzw, r12.xyzw
                texldl r15.xyzw, r15.xyzw, s4
                mov r13.xyzw, c31.xyzw
                mov r13.xy, r14.zwzz
                mov r16.xyzw, r13.xyzw
                texldl r16.xyzw, r16.xyzw, s5
                cmp r8.w, -r5.w, r16.x, r15.x
                add r9.w, r9.z, c42.z
                add r8.w, r8.w, -r9.w
                cmp r8.w, r8.w, c30.x, c32.w
                add r8.w, -r8.w, c32.w
                cmp r8.w, -r8.w, c30.x, c42.w
                mov r9.w, r3.z
                add r8.w, r9.w, r8.w
                mov r3.z, r8.w
                mov r8.w, r3.w
                add r8.w, r8.w, c32.w
                mov r3.w, r8.w
            endrep
            mov r5.w, r3.z
            mov r4.w, r5.w
        else
        endif
    else
    endif
    mov r5.w, r4.w
    mul r8.xyz, r8.xyzx, r5.w
    dp3 r5.w, r10.xyzx, c9.xyzx
    mov_sat r5.w, r5.w
    log r5.w, r5.w
    mul r5.w, r5.w, c43.x
    exp r5.w, r5.w
    mov r9.xyz, r5.w
    mul r9.xyz, c10.xyzx, r9.xyzx
    mul r9.xyz, r9.xyzx, c43.yzwy
    add r5.w, c28.y, c33.z
    cmp r5.w, r5.w, c30.x, c32.w
    mov r8.w, r4.w
    cmp r8.w, -r5.w, r8.w, c32.w
    mov r4.w, r8.w
    add r9.w, -r5.w, c32.w
    if_ne r9.w, -r9.w
        mul r10.xyz, r5.x, c20.xyzx
        mul r12.xyz, r5.y, c21.xyzx
        add r10.xyz, r10.xyzx, r12.xyzx
        mul r12.xyz, r5.z, c22.xyzx
        add r10.xyz, r10.xyzx, r12.xyzx
        add r10.xyz, r10.xyzx, c23.xyzx
        mul r12.xyz, r5.x, c24.xyzx
        mul r13.xyz, r5.y, c25.xyzx
        add r12.xyz, r12.xyzx, r13.xyzx
        mul r13.xyz, r5.z, c26.xyzx
        add r12.xyz, r12.xyzx, r13.xyzx
        add r12.xyz, r12.xyzx, c27.xyzx
        abs r11.zw, r10.xxxy
        max r9.w, r11.z, r11.w
        add r9.w, r9.w, c39.w
        cmp r9.w, r9.w, c30.x, c32.w
        cmp r10.xyz, -r9.w, r12.xyzx, r10.xyzx
        mul r10.w, r10.x, c29.x
        add r10.w, r10.w, c29.x
        mul r11.z, r10.y, c29.x
        add r11.z, -r11.z, c29.x
        mov r11.x, r10.w
        mov r11.y, r11.z
        mul r10.w, c28.z, c29.x
        add r11.xy, r11.xyxx, r10.w
        add r11.zw, r11.xxxy, c41.xxxy
        cmp r11.zw, r11.xxzw, c30.xxxy, c41.xxzw
        max r10.w, r11.z, r11.w
        add r11.zw, -r11.xxxy, c42.xxxy
        cmp r11.zw, r11.xxzw, c30.xxxy, c41.xxzw
        max r11.z, r11.z, r11.w
        max r10.w, r10.w, r11.z
        cmp r11.z, r10.z, c30.x, c32.w
        max r10.w, r10.w, r11.z
        add r11.z, -r10.z, c32.w
        cmp r11.z, r11.z, c30.x, c32.w
        max r10.w, r10.w, r11.z
        cmp r8.w, -r10.w, r8.w, c32.w
        mov r4.w, r8.w
        cmp r5.w, -r10.w, r5.w, c32.w
        add r5.w, -r5.w, c32.w
        if_ne r5.w, -r5.w
            mov r3.z, c30.x
            mov r3.w, c30.x
            rep i0.xyzw
                mov r5.w, r3.w
                add r5.w, r5.w, c40.w
                cmp r5.w, r5.w, c30.x, c32.w
                add r5.w, -r5.w, c32.w
                if_ne r5.w, -r5.w
                    break
                else
                endif
                mov r5.w, r3.w
                mul r5.w, r5.w, c29.x
                frc r5.w, r5.w
                mul r5.w, r5.w, c32.x
                add r5.w, r5.w, c33.z
                mov r8.w, r3.w
                mul r8.w, r8.w, c29.x
                frc r15.xyzw, r8.w
                add r8.w, r8.w, -r15.x
                add r8.w, r8.w, c33.z
                mov r14.x, r5.w
                mov r14.y, r8.w
                mov r11.zw, r14.xxxy
                mul r11.zw, r11.xxzw, c28.z
                add r11.zw, r11.xxxy, r11.xxzw
                mov r12.xyzw, c31.xyzw
                mov r12.xy, r11.zwzz
                mov r15.xyzw, r12.xyzw
                texldl r15.xyzw, r15.xyzw, s6
                mov r13.xyzw, c31.xyzw
                mov r13.xy, r11.zwzz
                mov r16.xyzw, r13.xyzw
                texldl r16.xyzw, r16.xyzw, s7
                cmp r5.w, -r9.w, r16.x, r15.x
                add r8.w, r10.z, c42.z
                add r5.w, r5.w, -r8.w
                cmp r5.w, r5.w, c30.x, c32.w
                add r5.w, -r5.w, c32.w
                cmp r5.w, -r5.w, c30.x, c42.w
                mov r8.w, r3.z
                add r5.w, r8.w, r5.w
                mov r3.z, r5.w
                mov r5.w, r3.w
                add r5.w, r5.w, c32.w
                mov r3.w, r5.w
            endrep
            mov r4.w, r3.z
        else
        endif
    else
    endif
    mov r3.z, r4.w
    mul r9.xyz, r9.xyzx, r3.z
    add r8.xyz, r8.xyzx, r9.xyzx
    add r3.z, -r6.w, c32.w
    mul r8.xyz, r8.xyzx, r3.z
    add r6.xyz, r6.xyzx, r8.xyzx
    mul r0.y, r0.y, c2.z
    mov_sat r0.y, r0.y
    mul r0.y, r0.y, r0.x
    add r6.xyz, r6.xyzx, -r1.xyzx
    mul r6.xyz, r0.y, r6.xyzx
    add r6.xyz, r1.xyzx, r6.xyzx
    rcp r3.z, c1.x
    rcp r3.w, c1.y
    mul r3.xy, r3.xyxx, r3.zwzz
    mov r4.xy, r3.xyxx
    mov r4.z, c1.z
    mov r3.xyz, r4.xyzx
    mul r3.xyz, r3.xyzx, r0.z
    mul r4.xyz, r3.x, c3.xyzx
    mul r8.xyz, r3.y, c4.xyzx
    add r4.xyz, r4.xyzx, r8.xyzx
    mul r3.xyz, r3.z, c5.xyzx
    add r3.xyz, r4.xyzx, r3.xyzx
    add r3.xyz, r3.xyzx, c6.xyzx
    add r0.y, r5.z, -r3.z
    mul r3.x, r2.x, r2.x
    mul r3.x, r3.x, c44.x
    rcp r3.y, c0.z
    mul r3.x, r3.x, r3.y
    add r0.z, r0.z, -r2.x
    add r0.z, r3.x, -r0.z
    cmp r0.z, r0.z, c30.x, c32.w
    add r2.x, -r0.y, c44.y
    cmp r2.x, r2.x, c30.x, c32.w
    min r0.z, r0.z, r2.x
    add r2.x, r0.y, c44.z
    cmp r2.x, r2.x, c30.x, c32.w
    min r0.z, r0.z, r2.x
    add r0.y, r0.y, c44.w
    mul r0.y, r0.y, c45.x
    mov_sat r0.y, r0.y
    mul r2.x, r0.y, r0.y
    mul r0.y, r0.y, c32.x
    add r0.y, -r0.y, c45.y
    mul r0.y, r2.x, r0.y
    add r0.y, -r0.y, c32.w
    cmp r0.y, -r0.z, c30.x, r0.y
    mul r0.z, r5.x, c45.z
    mul r2.x, r5.y, c45.w
    add r0.z, r0.z, r2.x
    mul r2.x, c2.y, c46.x
    add r0.z, r0.z, r2.x
    mad r0.z, r0.z, c36.z, c29.x
    frc r0.z, r0.z
    mad r0.z, r0.z, c36.y, c36.w
    sincos r2.y, r0.z
    mul r0.z, r2.y, c29.x
    add r0.z, r0.z, c29.x
    add r0.z, r0.z, c46.y
    mul r0.z, r0.z, c46.z
    mov_sat r0.z, r0.z
    mul r2.x, r0.z, r0.z
    mul r0.z, r0.z, c32.x
    add r0.z, -r0.z, c45.y
    mul r0.z, r2.x, r0.z
    mul r0.y, r0.y, r0.z
    mul r0.y, r0.y, c2.w
    mul r0.x, r0.y, r0.x
    mul r0.y, r0.w, c46.w
    add r0.y, -r0.y, c32.w
    mov_sat r0.y, r0.y
    mul r0.x, r0.x, r0.y
    mov_sat r0.x, r0.x
    add r0.yzw, c11.xxyz, -r6.xxyz
    mul r0.xyz, r0.x, r0.yzwy
    add r0.xyz, r6.xyzx, r0.xyzx
    mov r0.w, r1.w
    mov r7.xyzw, r0.xyzw
else
endif
mov oC0.xyzw, r7.xyzw
