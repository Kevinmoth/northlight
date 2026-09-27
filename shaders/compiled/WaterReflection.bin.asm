ps_3_0
dcl_texcoord0 v0
def c29 = 5.00000000e-01, 5.00000000e-01, -1.00000000e+00, -1.00000000e+00
def c30 = 0.00000000e+00, 0.00000000e+00, 9.99999975e-06, 1.20000001e-02
def c31 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c32 = 2.00000000e+00, -2.00000000e+00, -1.00000000e+00, 1.00000000e+00
def c33 = 6.99999975e-04, 3.00000003e-03, -5.00000000e-01, 1.10000002e+00
def c34 = 7.20000029e-01, 3.10000002e-01, -4.09999996e-01, 8.89999986e-01
def c35 = 7.30000019e-01, 1.73000002e+00, -1.21000004e+00, 1.63000000e+00
def c36 = 6.28318548e+00, 1.59154937e-01, -3.14159274e+00, 4.51999992e-01
def c37 = 5.49999997e-02, 5.49999997e-02, 3.50000001e-02, 3.50000001e-02
def c38 = 1.20000001e-02, 1.20000001e-02, 4.49999988e-01, 5.50000012e-01
def c39 = -2.40000000e+01, 2.00000009e-03, 1.80000007e-01, 2.80000001e-01
def c40 = 1.00000005e-03, 5.00000000e-01, -5.00000000e-01, 1.19999997e-01
def c41 = 1.00000000e+00, 1.00000000e+00, -3.99999991e-02, 9.95000005e-01
def c42 = 1.40000000e+01, 6.66666687e-01, 3.00000000e+00, 0.00000000e+00
defi i0 = 255, 0, 0, 0
dcl_2d s1
dcl_2d s2
dcl_2d s0
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
texldl r1.xyzw, r1.xyzw, s2
mov r2.xyzw, c31.xyzw
mov r2.xy, r0.xyxx
texldl r2.xyzw, r2.xyzw, s1
add r0.z, r2.x, -c1.w
mul r0.z, r0.z, c2.x
mov_sat r0.z, r0.z
mul r0.w, c0.z, c0.w
add r2.x, c0.w, -c0.z
mul r0.z, r0.z, r2.x
add r0.z, c0.w, -r0.z
max r0.z, r0.z, c30.z
rcp r0.z, r0.z
mul r0.z, r0.w, r0.z
max r2.y, r1.x, c0.z
mul r2.zw, c0.xxxy, c29.xxxy
add r0.xy, r0.xyxx, -r2.zwzz
mul r0.xy, r0.xyxx, c32.xyxx
add r0.xy, r0.xyxx, c32.zwzz
rcp r3.x, c1.x
rcp r3.y, c1.y
mul r0.xy, r0.xyxx, r3.xyxx
mov r3.xy, r0.xyxx
mov r3.z, c1.z
mov r4.xyz, r3.xyzx
mul r4.xyz, r4.xyzx, r2.y
mul r5.xyz, r4.x, c3.xyzx
mul r6.xyz, r4.y, c4.xyzx
add r5.xyz, r5.xyzx, r6.xyzx
mul r6.xyz, r4.z, c5.xyzx
add r5.xyz, r5.xyzx, r6.xyzx
add r5.xyz, r5.xyzx, c6.xyzx
dsx r6.xyz, r5.xyzx
dsy r7.xyz, r5.xyzx
dp3 r0.x, r6.xyzx, r6.xyzx
dp3 r0.y, r7.xyzx, r7.xyzx
max r0.x, r0.x, r0.y
add r0.y, c0.z, -r1.x
cmp r0.y, r0.y, c30.x, c32.w
mul r2.y, r1.x, c33.x
max r2.y, r2.y, c30.w
add r0.z, r0.z, r2.y
add r0.z, r0.z, -r1.x
cmp r0.z, r0.z, c30.x, c32.w
add r0.z, -r0.z, c32.w
min r0.y, r0.y, r0.z
add r0.z, -r1.y, c33.y
cmp r0.z, r0.z, c30.x, c32.w
min r0.y, r0.y, r0.z
cmp r0.y, -r0.y, c30.x, c32.w
add r0.y, r0.y, c33.z
cmp r0.y, r0.y, c30.x, c32.w
mov r1.xyzw, r6.xyzw
cmp r1.xyzw, -r0.y, r1.xyzw, c31.xyzw
mov r6.xyzw, r1.xyzw
add r0.y, -r0.y, c32.w
if_ne r0.y, -r0.y
    dp2add r0.y, r5.xyxx, c34.xyxx, c30.x
    mul r0.z, c2.y, c33.w
    add r0.y, r0.y, -r0.z
    dp2add r0.z, r5.xyxx, c34.zwzz, c30.x
    mul r1.x, c2.y, c35.x
    add r0.z, r0.z, r1.x
    dp2add r1.x, r5.xyxx, c35.yzyy, c30.x
    mul r1.y, c2.y, c35.w
    add r1.x, r1.x, -r1.y
    mad r0.y, r0.y, c36.y, c29.x
    frc r0.y, r0.y
    mad r0.y, r0.y, c36.x, c36.z
    sincos r7.x, r0.y
    mov r1.yz, r7.x
    mul r1.yz, r1.xyzx, c34.xxyx
    mul r1.yz, r1.xyzx, c37.xxyx
    mad r0.y, r0.z, c36.y, c29.x
    frc r0.y, r0.y
    mad r0.y, r0.y, c36.x, c36.z
    sincos r7.x, r0.y
    mov r0.yz, r7.x
    mul r0.yz, r0.xyzx, c34.xzwx
    mul r0.yz, r0.xyzx, c37.xzwx
    add r0.yz, r1.xyzx, r0.xyzx
    mad r1.x, r1.x, c36.y, c29.x
    frc r1.x, r1.x
    mad r1.x, r1.x, c36.x, c36.z
    sincos r1.x, r1.x
    mov r1.xy, r1.x
    mul r1.xy, r1.xyxx, c35.yzyy
    mul r1.xy, r1.xyxx, c38.xyxx
    add r0.yz, r0.xyzx, r1.xxyx
    mul r0.x, r0.x, c36.w
    add r0.x, -r0.x, c32.w
    mov_sat r0.x, r0.x
    mul r0.xy, r0.yzyy, r0.x
    mov r0.xy, -r0.xyxx
    mov r0.z, c32.w
    dp3 r1.x, r0.xyzx, r0.xyzx
    rsq r1.x, r1.x
    mov r1.xyz, r1.x
    mul r0.xyz, r1.xyzx, r0.xyzx
    add r1.xyz, r5.xyzx, -c6.xyzx
    dp3 r1.w, r1.xyzx, r1.xyzx
    rsq r1.w, r1.w
    mov r7.xyz, r1.w
    mul r1.xyz, r7.xyzx, r1.xyzx
    dp3 r1.w, r0.xyzx, -r1.xyzx
    cmp r1.w, r1.w, c30.x, c32.w
    cmp r0.xyz, -r1.w, r0.xyzx, -r0.xyzx
    dp3 r1.w, r1.xyzx, r0.xyzx
    add r1.w, r1.w, r1.w
    mul r0.xyz, r0.xyzx, r1.w
    add r0.xyz, r1.xyzx, -r0.xyzx
    dp3 r1.x, r0.xyzx, c3.xyzx
    dp3 r1.y, r0.xyzx, c4.xyzx
    dp3 r1.z, r0.xyzx, c5.xyzx
    mov r7.x, r1.x
    mov r7.y, r1.y
    mov r7.z, r1.z
    max r1.xyz, c11.xyzx, c31.xyzx
    mov_sat r1.w, r0.z
    mul r1.w, r1.w, c38.w
    add r1.w, r1.w, c38.z
    mul r1.xyz, r1.xyzx, r1.w
    mov r8.xyz, r1.xyzx
    mov r1.w, c30.x
    mov r2.y, c30.x
    mov r3.w, c29.z
    mov r4.w, c30.x
    rep i0.xyzw
        mov r18.y, r4.w
        add r18.y, r18.y, c39.x
        cmp r18.y, r18.y, c30.x, c32.w
        add r18.y, -r18.y, c32.w
        if_ne r18.y, -r18.y
            break
        else
        endif
        add r18.y, -r0.z, c39.y
        cmp r18.y, r18.y, c30.x, c32.w
        add r18.y, -r18.y, c32.w
        if_ne r18.y, -r18.y
            break
        else
        endif
        mov r18.y, r4.w
        mul r18.y, r18.y, c39.w
        add r18.y, r18.y, c39.z
        mov r18.z, r4.w
        mov r18.w, r4.w
        mul r18.z, r18.z, r18.w
        mul r18.z, r18.z, c37.x
        add r18.y, r18.y, r18.z
        mov r19.xyz, r7.xyzx
        mul r19.xyz, r19.xyzx, r18.y
        add r19.xyz, r4.xyzx, r19.xyzx
        mul r18.z, r19.z, c1.z
        mul r19.xy, r19.xyxx, c1.xyxx
        max r18.w, r18.z, c40.x
        rcp r15.z, r18.w
        rcp r15.w, r18.w
        mul r19.xy, r19.xyxx, r15.zwzz
        mul r19.xy, r19.xyxx, c40.yzyy
        add r19.xy, r19.xyxx, c29.xyxx
        add r19.xy, r19.xyxx, r2.zwzz
        add r18.w, c0.z, -r18.z
        cmp r18.w, r18.w, c30.x, c32.w
        add r18.w, -r18.w, c32.w
        cmp r19.zw, r19.xxxy, c30.xxxy, c41.xxxy
        max r19.z, r19.z, r19.w
        max r18.w, r18.w, r19.z
        add r19.zw, -r19.xxxy, c41.xxxy
        cmp r19.zw, r19.xxzw, c30.xxxy, c41.xxxy
        max r19.z, r19.z, r19.w
        max r18.w, r18.w, r19.z
        if_ne r18.w, -r18.w
            break
        else
        endif
        mov r11.xyzw, c31.xyzw
        mov r11.xy, r19.xyxx
        mov r19.xyzw, r11.xyzw
        texldl r19.xyzw, r19.xyzw, s1
        add r18.w, r19.x, -c1.w
        mul r18.w, r18.w, c2.x
        mov_sat r18.w, r18.w
        mul r18.w, r18.w, r2.x
        add r18.w, c0.w, -r18.w
        max r18.w, r18.w, c30.z
        rcp r13.w, r18.w
        mul r18.w, r0.w, r13.w
        add r18.z, r18.z, -r18.w
        cmp r18.w, r18.z, c30.x, c32.w
        add r18.w, -r18.w, c32.w
        mov r19.x, r3.w
        cmp r19.x, r19.x, c30.x, c32.w
        min r18.w, r18.w, r19.x
        if_ne r18.w, -r18.w
            mov r18.w, r2.y
            add r19.x, r18.w, r18.y
            mul r19.x, r19.x, c29.x
            mov r19.yzw, r7.xxyz
            mul r19.yzw, r19.xyzw, r19.x
            add r19.yzw, r4.xxyz, r19.xyzw
            mul r20.xy, r19.yzyy, c1.xyxx
            mul r19.y, r19.w, c1.z
            max r19.z, r19.y, c40.x
            rcp r12.z, r19.z
            rcp r12.w, r19.z
            mul r19.zw, r20.xxxy, r12.xxzw
            mul r19.zw, r19.xxzw, c40.xxyz
            add r19.zw, r19.xxzw, c29.xxxy
            add r19.zw, r19.xxzw, r2.xxzw
            mov r14.xyzw, c31.xyzw
            mov r14.xy, r19.zwzz
            mov r20.xyzw, r14.xyzw
            texldl r20.xyzw, r20.xyzw, s1
            add r19.z, r20.x, -c1.w
            mul r19.z, r19.z, c2.x
            mov_sat r19.z, r19.z
            mul r19.z, r19.z, r2.x
            add r19.z, c0.w, -r19.z
            max r19.z, r19.z, c30.z
            rcp r18.x, r19.z
            mul r19.z, r0.w, r18.x
            add r19.y, r19.z, -r19.y
            cmp r19.y, r19.y, c30.x, c32.w
            cmp r19.z, -r19.y, r18.y, r19.x
            cmp r18.w, -r19.y, r19.x, r18.w
            add r19.x, r18.w, r19.z
            mul r19.x, r19.x, c29.x
            mov r20.xyz, r7.xyzx
            mul r20.xyz, r20.xyzx, r19.x
            add r20.xyz, r4.xyzx, r20.xyzx
            mul r19.yw, r20.xxxy, c1.xxxy
            mul r20.x, r20.z, c1.z
            max r20.y, r20.x, c40.x
            rcp r15.x, r20.y
            rcp r15.y, r20.y
            mul r19.yw, r19.xyxw, r15.xxxy
            mul r19.yw, r19.xyxw, c40.xyxz
            add r19.yw, r19.xyxw, c29.xxxy
            add r19.yw, r19.xyxw, r2.xzxw
            mov r14.xyzw, c31.xyzw
            mov r14.xy, r19.ywyy
            mov r21.xyzw, r14.xyzw
            texldl r21.xyzw, r21.xyzw, s1
            add r19.y, r21.x, -c1.w
            mul r19.y, r19.y, c2.x
            mov_sat r19.y, r19.y
            mul r19.y, r19.y, r2.x
            add r19.y, c0.w, -r19.y
            max r19.y, r19.y, c30.z
            rcp r13.z, r19.y
            mul r19.y, r0.w, r13.z
            add r19.y, r19.y, -r20.x
            cmp r19.y, r19.y, c30.x, c32.w
            cmp r19.z, -r19.y, r19.z, r19.x
            cmp r18.w, -r19.y, r19.x, r18.w
            add r18.w, r18.w, r19.z
            mul r18.w, r18.w, c29.x
            mov r19.xyw, r7.xyxz
            mul r19.xyw, r19.xyxw, r18.w
            add r19.xyw, r4.xyxz, r19.xyxw
            mul r20.xy, r19.xyxx, c1.xyxx
            mul r19.x, r19.w, c1.z
            max r19.y, r19.x, c40.x
            rcp r13.x, r19.y
            rcp r13.y, r19.y
            mul r19.yw, r20.xxxy, r13.xxxy
            mul r19.yw, r19.xyxw, c40.xyxz
            add r19.yw, r19.xyxw, c29.xxxy
            add r19.yw, r19.xyxw, r2.xzxw
            mov r14.xyzw, c31.xyzw
            mov r14.xy, r19.ywyy
            mov r20.xyzw, r14.xyzw
            texldl r20.xyzw, r20.xyzw, s1
            add r19.y, r20.x, -c1.w
            mul r19.y, r19.y, c2.x
            mov_sat r19.y, r19.y
            mul r19.y, r19.y, r2.x
            add r19.y, c0.w, -r19.y
            max r19.y, r19.y, c30.z
            rcp r8.w, r19.y
            mul r19.y, r0.w, r8.w
            add r19.x, r19.y, -r19.x
            cmp r19.x, r19.x, c30.x, c32.w
            cmp r18.w, -r19.x, r19.z, r18.w
            mov r19.xyz, r7.xyzx
            mul r19.xyz, r19.xyzx, r18.w
            add r19.xyz, r4.xyzx, r19.xyzx
            mul r20.xy, r19.xyxx, c1.xyxx
            mul r19.x, r19.z, c1.z
            max r19.y, r19.x, c40.x
            rcp r12.x, r19.y
            rcp r12.y, r19.y
            mul r19.yz, r20.xxyx, r12.xxyx
            mul r19.yz, r19.xyzx, c40.xyzx
            add r19.yz, r19.xyzx, c29.xxyx
            add r19.yz, r19.xyzx, r2.xzwx
            rcp r10.x, c0.x
            rcp r10.y, c0.y
            add r20.xy, r10.xyxx, c29.xyxx
            frc r21.xyzw, r20.xyxx
            add r20.xy, r20.xyxx, -r21.xyzw
            mul r19.yz, r19.xyzx, r20.xxyx
            frc r21.xyzw, r19.yzyy
            add r19.yz, r19.xyzx, -r21.xxyx
            add r20.xy, r20.xyxx, c29.zwzz
            max r19.yz, r19.xyzx, c30.xxyx
            min r19.yz, r19.xyzx, r20.xxyx
            add r19.yz, r19.xyzx, c29.xxyx
            mul r19.yz, r19.xyzx, c0.xxyx
            mov r9.xyzw, c31.xyzw
            mov r9.xy, r19.yzyy
            mov r20.xyzw, r9.xyzw
            texldl r20.xyzw, r20.xyzw, s1
            add r19.w, r20.x, -c1.w
            mul r19.w, r19.w, c2.x
            mov_sat r19.w, r19.w
            mul r19.w, r19.w, r2.x
            add r19.w, c0.w, -r19.w
            max r19.w, r19.w, c30.z
            rcp r5.w, r19.w
            mul r19.w, r0.w, r5.w
            mov r16.xyzw, c31.xyzw
            mov r16.xy, r19.yzyy
            mov r20.xyzw, r16.xyzw
            texldl r20.xyzw, r20.xyzw, s2
            mul r18.w, r18.w, c37.z
            max r18.w, r18.w, c40.w
            add r19.x, r19.x, -r19.w
            abs r19.x, r19.x
            add r21.xy, r19.yzyy, -r2.zwzz
            mul r21.xy, r21.xyxx, c32.xyxx
            add r21.xy, r21.xyxx, c32.zwzz
            rcp r10.z, c1.x
            rcp r10.w, c1.y
            mul r21.xy, r21.xyxx, r10.zwzz
            mov r3.xy, r21.xyxx
            mov r3.z, c1.z
            mov r21.xyz, r3.xyzx
            mul r21.xyz, r21.xyzx, r19.w
            mul r22.xyz, r21.x, c3.xyzx
            mul r23.xyz, r21.y, c4.xyzx
            add r22.xyz, r22.xyzx, r23.xyzx
            mul r21.xyz, r21.z, c5.xyzx
            add r21.xyz, r22.xyzx, r21.xyzx
            add r21.xyz, r21.xyzx, c6.xyzx
            add r21.w, r19.x, -r18.w
            cmp r21.w, r21.w, c30.x, c32.w
            add r22.x, r5.z, c41.z
            add r21.x, r21.z, -r22.x
            cmp r21.x, r21.x, c30.x, c32.w
            add r21.x, -r21.x, c32.w
            min r21.x, r21.w, r21.x
            mul r21.y, c0.w, c41.w
            add r21.y, r19.w, -r21.y
            cmp r21.y, r21.y, c30.x, c32.w
            min r21.x, r21.x, r21.y
            add r21.y, c0.z, -r20.x
            cmp r21.y, r21.y, c30.x, c32.w
            mul r21.z, r20.x, c33.x
            max r21.z, r21.z, c30.w
            add r19.w, r19.w, r21.z
            add r19.w, r19.w, -r20.x
            cmp r19.w, r19.w, c30.x, c32.w
            add r19.w, -r19.w, c32.w
            min r19.w, r21.y, r19.w
            add r20.x, -r20.y, c33.y
            cmp r20.x, r20.x, c30.x, c32.w
            min r19.w, r19.w, r20.x
            cmp r19.w, -r19.w, c30.x, c32.w
            add r19.w, r19.w, c33.z
            cmp r19.w, r19.w, c30.x, c32.w
            min r19.w, r21.x, r19.w
            if_ne r19.w, -r19.w
                min r19.w, r19.y, r19.z
                add r20.x, -r19.y, c32.w
                add r20.y, -r19.z, c32.w
                min r20.x, r20.x, r20.y
                min r19.w, r19.w, r20.x
                mul r19.w, r19.w, c42.x
                mov_sat r19.w, r19.w
                mul r20.x, r18.y, c42.y
                mov_sat r20.x, r20.x
                mul r19.w, r19.w, r20.x
                mul r20.x, r18.w, c29.x
                add r19.x, r19.x, -r20.x
                add r18.w, r18.w, -r20.x
                rcp r7.w, r18.w
                mul r18.w, r19.x, r7.w
                mov_sat r18.w, r18.w
                mul r19.x, r18.w, r18.w
                mul r18.w, r18.w, c32.x
                add r18.w, -r18.w, c42.z
                mul r18.w, r19.x, r18.w
                add r18.w, -r18.w, c32.w
                mul r18.w, r19.w, r18.w
                mov r1.w, r18.w
                mov r17.xyzw, c31.xyzw
                mov r17.xy, r19.yzyy
                mov r19.xyzw, r17.xyzw
                texldl r19.xyzw, r19.xyzw, s0
                add r19.xyz, r19.xyzx, -r1.xyzx
                mul r19.xyz, r18.w, r19.xyzx
                add r19.xyz, r1.xyzx, r19.xyzx
                mov r8.xyz, r19.xyzx
            else
            endif
            break
        else
        endif
        mov r2.y, r18.y
        mov r3.w, r18.z
        mov r18.y, r4.w
        add r18.y, r18.y, c32.w
        mov r4.w, r18.y
    endrep
    mov r0.xyz, r8.xyzx
    mov r0.w, r1.w
    mov r1.xyz, r0.xyzx
    mov r1.w, r0.w
    mov r0.xyzw, r1.xyzw
    mov r6.xyzw, r0.xyzw
else
endif
mov oC0.xyzw, r6.xyzw
