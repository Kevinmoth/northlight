ps_3_0
dcl_texcoord0 v0
def c68 = 5.00000000e-01, 5.00000000e-01, -1.00000000e+00, -1.00000000e+00
def c69 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c70 = 9.99999975e-06, 1.00000000e+00, -9.99989986e-01, 3.90625000e-03
def c71 = 1.00000005e-03, 1.50000006e-01, 1.50000006e-01, 1.50000006e-01
def c72 = 9.99999975e-05, 9.99999975e-05, 9.99999975e-05, 3.00000000e+00
def c73 = 1.60000000e+01, -1.44269502e+00, 2.50000000e-01, 2.99999993e-02
def c74 = 2.00000000e+00, -4.49999988e-01, -4.49999988e-01, -4.49999988e-01
def c75 = -2.40000000e+01, 4.16666679e-02, -7.37368822e-01, 6.75490320e-01
def c76 = 7.37368822e-01, 2.00000000e+00, -2.00000000e+00, 0.00000000e+00
def c77 = -1.00000000e+00, 1.00000000e+00, 5.00000000e-01, -5.00000000e-01
def c78 = 1.00000000e+00, 1.00000000e+00, -1.00000000e+00, 0.00000000e+00
def c79 = 1.00000000e+00, 0.00000000e+00, 0.00000000e+00, -1.00000000e+00
def c80 = 0.00000000e+00, 1.00000000e+00, 0.00000000e+00, 0.00000000e+00
defi i0 = 255, 0, 0, 0
dcl_2d s12
dcl_2d s1
dcl_2d s15
dcl_2d s14
dcl_2d s8
dcl_2d s0
mul r0.xy, v0.xyxx, c33.zwzz
frc r1.xyzw, r0.xyxx
add r0.xy, r0.xyxx, -r1.xyzw
add r0.zw, r0.xxxy, c68.xxxy
rcp r1.x, c33.z
rcp r1.y, c33.w
mul r1.xy, r0.zwzz, r1.xyxx
mov r2.xyzw, c69.xyzw
mov r2.xy, r1.xyxx
texldl r2.xyzw, r2.xyzw, s8
mov r3.xyzw, r2.xyzw
mul r1.zw, r1.xxxy, c33.xxxy
frc r4.xyzw, r1.zwzz
add r1.zw, r1.xxzw, -r4.xxxy
add r4.xy, c33.xyxx, c68.zwzz
max r1.zw, r1.xxzw, c69.xxxy
min r1.zw, r1.xxzw, r4.xxxy
add r1.zw, r1.xxzw, c68.xxxy
mul r1.zw, r1.xxzw, c0.xxxy
mov r5.xyzw, c69.xyzw
mov r5.xy, r1.zwzz
texldl r5.xyzw, r5.xyzw, s1
add r4.z, r5.x, -c1.w
mul r4.z, r4.z, c2.x
mov_sat r4.z, r4.z
mul r4.w, c0.z, c0.w
add r5.x, c0.w, -c0.z
mul r5.y, r4.z, r5.x
add r5.y, c0.w, -r5.y
max r5.y, r5.y, c70.x
rcp r5.z, r5.y
mul r5.z, r4.w, r5.z
cmp r5.w, -c30.y, c69.x, c70.y
add r4.z, r4.z, c70.z
cmp r4.z, r4.z, c69.x, c70.y
min r5.w, r5.w, r4.z
if_ne r5.w, -r5.w
    mul r5.w, r2.w, c30.z
    add r5.w, -r5.w, c70.y
    rcp r6.x, c20.z
    mul r5.w, r5.w, r6.x
    mov_sat r5.w, r5.w
    mov r6.xyz, r2.xyzx
    add r6.w, -r5.w, c70.w
    cmp r6.w, r6.w, c69.x, c70.y
    if_ne r6.w, -r6.w
        mul r6.w, r5.z, c1.z
        mad r6.w, r6.w, c25.x, c25.y
        max r6.w, r6.w, c69.x
        log r6.w, r6.w
        mul r6.w, c25.z, r6.w
        exp r6.w, r6.w
        mov_sat r6.w, r6.w
        add r6.w, r6.w, c68.z
        mad r6.w, c25.w, r6.w, c70.y
        max r6.w, r6.w, c71.x
        add r7.x, -r6.w, c70.y
        mul r7.xyz, r7.x, c26.xyzx
        mov r8.xyzw, c69.xyzw
        mov r8.xy, r1.xyxx
        texldl r8.xyzw, r8.xyzw, s12
        mov r9.xyzw, c69.xyzw
        mov r9.xy, r1.xyxx
        texldl r9.xyzw, r9.xyzw, s0
        max r8.xyz, r8.xyzx, c71.yzwy
        mul r8.xyz, r6.w, r8.xyzx
        min r10.xyz, r7.xyzx, r9.xyzx
        add r9.xyz, r9.xyzx, -r10.xyzx
        max r8.xyz, r8.xyzx, r9.xyzx
        max r8.xyz, r8.xyzx, c72.xyzx
        mul r9.xyz, r2.xyzx, r6.w
        rcp r10.x, r8.x
        rcp r10.y, r8.y
        rcp r10.z, r8.z
        mul r9.xyz, r9.xyzx, r10.xyzx
        rcp r1.x, r5.z
        mul r1.x, c30.w, r1.x
        max r1.x, r1.x, c72.w
        min r1.x, r1.x, c73.x
        mul r1.y, r5.z, c73.w
        max r1.y, r1.y, c73.z
        rcp r1.y, r1.y
        mul r1.y, r1.y, c73.y
        mul r7.w, c30.z, c74.x
        rcp r8.w, c20.z
        mul r7.w, r7.w, r8.w
        max r10.xyz, r9.xyzx, c74.yzwy
        mov r8.w, c70.y
        mov r9.w, c68.x
        mov r11.x, c70.y
        mov r11.y, c69.x
        mov r10.w, c69.x
        rep i0.xyzw
            mov r16.w, r10.w
            add r16.w, r16.w, c75.x
            cmp r16.w, r16.w, c69.x, c70.y
            add r16.w, -r16.w, c70.y
            if_ne r16.w, -r16.w
                break
            else
            endif
            mov r18.zw, r11.xxxy
            mov r16.w, r9.w
            mul r16.w, r16.w, c75.y
            rsq r16.z, r16.w
            rcp r14.w, r16.z
            mul r16.w, r14.w, r1.x
            mul r18.zw, r18.xxzw, r16.w
            add r18.zw, r0.xxzw, r18.xxzw
            frc r19.xyzw, r18.zwzz
            add r18.zw, r18.xxzw, -r19.xxxy
            add r19.xy, c33.zwzz, c68.zwzz
            max r18.zw, r18.xxzw, c69.xxxy
            min r18.zw, r18.xxzw, r19.xxxy
            add r18.zw, r18.xxzw, c68.xxxy
            rcp r16.x, c33.z
            rcp r16.y, c33.w
            mul r18.zw, r18.xxzw, r16.xxxy
            mov r15.xyzw, c69.xyzw
            mov r15.xy, r18.zwzz
            mov r19.xyzw, r15.xyzw
            texldl r19.xyzw, r19.xyzw, s8
            mul r20.xy, r18.zwzz, c33.xyxx
            frc r21.xyzw, r20.xyxx
            add r20.xy, r20.xyxx, -r21.xyzw
            max r20.xy, r20.xyxx, c69.xyxx
            min r20.xy, r20.xyxx, r4.xyxx
            add r20.xy, r20.xyxx, c68.xyxx
            mul r20.xy, r20.xyxx, c0.xyxx
            mov r17.xyzw, c69.xyzw
            mov r17.xy, r20.xyxx
            mov r20.xyzw, r17.xyzw
            texldl r20.xyzw, r20.xyzw, s1
            add r16.w, r20.x, -c1.w
            mul r16.w, r16.w, c2.x
            mov_sat r16.w, r16.w
            mul r16.w, r16.w, r5.x
            add r16.w, c0.w, -r16.w
            max r16.w, r16.w, c70.x
            rcp r11.w, r16.w
            mul r16.w, r4.w, r11.w
            add r16.w, r16.w, -r5.z
            abs r16.w, r16.w
            mul r16.w, r16.w, r1.y
            exp r11.z, r16.w
            add r16.w, r19.w, -r2.w
            abs r16.w, r16.w
            mul r16.w, r16.w, r7.w
            add r16.w, -r16.w, c70.y
            mov_sat r16.w, r16.w
            mul r16.w, r11.z, r16.w
            mul r19.xyz, r19.xyzx, r6.w
            mov r13.xyzw, c69.xyzw
            mov r13.xy, r18.zwzz
            mov r20.xyzw, r13.xyzw
            texldl r20.xyzw, r20.xyzw, s12
            mov r12.xyzw, c69.xyzw
            mov r12.xy, r18.zwzz
            mov r21.xyzw, r12.xyzw
            texldl r21.xyzw, r21.xyzw, s0
            max r20.xyz, r20.xyzx, c71.yzwy
            mul r20.xyz, r6.w, r20.xyzx
            min r22.xyz, r7.xyzx, r21.xyzx
            add r21.xyz, r21.xyzx, -r22.xyzx
            max r20.xyz, r20.xyzx, r21.xyzx
            max r20.xyz, r20.xyzx, c72.xyzx
            rcp r14.x, r20.x
            rcp r14.y, r20.y
            rcp r14.z, r20.z
            mul r19.xyz, r19.xyzx, r14.xyzx
            max r19.xyz, r19.xyzx, c74.yzwy
            mul r19.xyz, r19.xyzx, r16.w
            mov r20.xyz, r10.xyzx
            add r19.xyz, r20.xyzx, r19.xyzx
            mov r10.xyz, r19.xyzx
            mov r18.z, r8.w
            add r16.w, r18.z, r16.w
            mov r8.w, r16.w
            mov r18.zw, r11.xxxy
            mul r16.w, r18.z, c75.z
            mov r18.zw, r11.xxxy
            mul r18.z, r18.w, c75.w
            add r16.w, r16.w, -r18.z
            mov r18.zw, r11.xxxy
            mul r18.z, r18.z, c75.w
            mov r19.xy, r11.xyxx
            mul r18.w, r19.y, c76.x
            add r18.z, r18.z, -r18.w
            mov r18.x, r16.w
            mov r18.y, r18.z
            mov r18.zw, r18.xxxy
            mov r11.xy, r18.zwzz
            mov r16.w, r9.w
            add r16.w, r16.w, c70.y
            mov r9.w, r16.w
            mov r16.w, r10.w
            add r16.w, r16.w, c70.y
            mov r10.w, r16.w
        endrep
        mov r7.xyz, r10.xyzx
        mov r1.x, r8.w
        rcp r10.x, r1.x
        rcp r10.y, r1.x
        rcp r10.z, r1.x
        mul r7.xyz, r7.xyzx, r10.xyzx
        add r7.xyz, r7.xyzx, -r9.xyzx
        mul r7.xyz, r5.w, r7.xyzx
        add r7.xyz, r9.xyzx, r7.xyzx
        mul r7.xyz, r7.xyzx, r8.xyzx
        rcp r8.x, r6.w
        rcp r8.y, r6.w
        rcp r8.z, r6.w
        mul r7.xyz, r7.xyzx, r8.xyzx
        mov r6.xyz, r7.xyzx
    else
    endif
    mov r3.xyz, r6.xyzx
else
endif
mov r6.xyzw, r3.xyzw
mov r7.x, r5.z
mov r7.y, c69.x
mov r7.z, c69.x
mov r7.w, c70.y
cmp r1.x, -c57.x, c69.x, c70.y
add r1.x, -r1.x, c70.y
add r1.y, -r4.z, c70.y
max r1.x, r1.x, r1.y
mov r8.xyzw, r9.xyzw
cmp r8.xyzw, -r1.x, r8.xyzw, r6.xyzw
mov r9.xyzw, r8.xyzw
mov r10.xyzw, r11.xyzw
cmp r10.xyzw, -r1.x, r10.xyzw, r7.xyzw
mov r11.xyzw, r10.xyzw
add r1.y, -r1.x, c70.y
if_ne r1.y, -r1.y
    rcp r1.y, r5.y
    mul r1.y, r4.w, r1.y
    mul r4.xy, c0.xyxx, c68.xyxx
    add r1.zw, r1.xxzw, -r4.xxxy
    mul r1.zw, r1.xxzw, c76.xxyz
    add r1.zw, r1.xxzw, c77.xxxy
    rcp r4.x, c1.x
    rcp r4.y, c1.y
    mul r1.zw, r1.xxzw, r4.xxxy
    mov r4.xy, r1.zwzz
    mov r4.z, c1.z
    mul r1.yzw, r4.xxyz, r1.y
    mul r4.xyz, r1.y, c3.xyzx
    mul r5.xyz, r1.z, c4.xyzx
    add r4.xyz, r4.xyzx, r5.xyzx
    mul r1.yzw, r1.w, c5.xxyz
    add r1.yzw, r4.xxyz, r1.xyzw
    add r1.yzw, r1.xyzw, c6.xxyz
    mul r4.xyz, r1.y, c53.xyzx
    mul r5.xyz, r1.z, c54.xyzx
    add r4.xyz, r4.xyzx, r5.xyzx
    mul r1.yzw, r1.w, c55.xxyz
    add r1.yzw, r4.xxyz, r1.xyzw
    add r1.yzw, r1.xyzw, c56.xxyz
    mul r4.x, r1.w, c1.z
    add r4.y, c0.z, -r4.x
    cmp r4.y, r4.y, c69.x, c70.y
    add r4.y, -r4.y, c70.y
    cmp r5.xyzw, -r4.y, r8.xyzw, r6.xyzw
    mov r9.xyzw, r5.xyzw
    cmp r8.xyzw, -r4.y, r10.xyzw, r7.xyzw
    mov r11.xyzw, r8.xyzw
    cmp r1.x, -r4.y, r1.x, c70.y
    add r4.y, -r1.x, c70.y
    if_ne r4.y, -r4.y
        mul r1.yz, r1.xyzx, c1.xxyx
        rcp r4.y, r4.x
        rcp r4.z, r4.x
        mul r1.yz, r1.xyzx, r4.xyzx
        mul r1.yz, r1.xyzx, c77.xzwx
        add r1.yz, r1.xyzx, c68.xxyx
        cmp r4.yz, r1.xyzx, c69.xxyx, c78.xxyx
        max r1.w, r4.y, r4.z
        add r4.yz, -r1.xyzx, c78.xxyx
        cmp r4.yz, r4.xyzx, c69.xxyx, c78.xxyx
        max r4.y, r4.y, r4.z
        max r1.w, r1.w, r4.y
        cmp r5.xyzw, -r1.w, r5.xyzw, r6.xyzw
        mov r9.xyzw, r5.xyzw
        cmp r8.xyzw, -r1.w, r8.xyzw, r7.xyzw
        mov r11.xyzw, r8.xyzw
        cmp r1.x, -r1.w, r1.x, c70.y
        add r1.w, -r1.x, c70.y
        if_ne r1.w, -r1.w
            mul r1.yz, r1.xyzx, c33.xzwx
            frc r10.xyzw, r1.yzyy
            add r4.yz, r1.xyzx, -r10.xxyx
            add r10.xy, c33.zwzz, c68.zwzz
            max r4.yz, r4.xyzx, c69.xxyx
            min r4.yz, r4.xyzx, r10.xxyx
            add r4.yz, r4.xyzx, c68.xxyx
            rcp r10.z, c33.z
            rcp r10.w, c33.w
            mul r4.yz, r4.xyzx, r10.xzwx
            mov r12.xyzw, c69.xyzw
            mov r12.xy, r4.yzyy
            texldl r12.xyzw, r12.xyzw, s15
            mul r1.w, r4.x, c73.w
            max r1.w, r1.w, c73.z
            add r4.x, r12.x, -r4.x
            abs r4.x, r4.x
            add r1.w, r1.w, -r4.x
            cmp r1.w, r1.w, c69.x, c70.y
            cmp r5.xyzw, -r1.w, r5.xyzw, r6.xyzw
            mov r9.xyzw, r5.xyzw
            cmp r5.xyzw, -r1.w, r8.xyzw, r7.xyzw
            mov r11.xyzw, r5.xyzw
            cmp r1.x, -r1.w, r1.x, c70.y
            add r1.x, -r1.x, c70.y
            if_ne r1.x, -r1.x
                mov r5.xyzw, c69.xyzw
                mov r5.xy, r4.yzyy
                mov r4.xyzw, r5.xyzw
                texldl r4.xyzw, r4.xyzw, s14
                mov r5.xyzw, r3.xyzw
                mov r5.w, r2.w
                mov r6.xyzw, r3.xyzw
                mov r6.w, r2.w
                add r1.xw, r0.xxxy, c78.zxxw
                max r1.xw, r1.xxxw, c69.xxxy
                min r1.xw, r1.xxxw, r10.xxxy
                add r1.xw, r1.xxxw, c68.xxxy
                rcp r2.x, c33.z
                rcp r2.y, c33.w
                mul r1.xw, r1.xxxw, r2.xxxy
                mov r2.xyzw, c69.xyzw
                mov r2.xy, r1.xwxx
                mov r8.xyzw, r2.xyzw
                texldl r8.xyzw, r8.xyzw, s8
                min r5.xyzw, r5.xyzw, r8.xyzw
                max r6.xyzw, r6.xyzw, r8.xyzw
                add r1.xw, r0.xxxy, c79.xxxy
                max r1.xw, r1.xxxw, c69.xxxy
                min r1.xw, r1.xxxw, r10.xxxy
                add r1.xw, r1.xxxw, c68.xxxy
                rcp r8.x, c33.z
                rcp r8.y, c33.w
                mul r1.xw, r1.xxxw, r8.xxxy
                mov r2.xyzw, c69.xyzw
                mov r2.xy, r1.xwxx
                mov r8.xyzw, r2.xyzw
                texldl r8.xyzw, r8.xyzw, s8
                min r5.xyzw, r5.xyzw, r8.xyzw
                max r6.xyzw, r6.xyzw, r8.xyzw
                add r1.xw, r0.xxxy, c79.zxxw
                max r1.xw, r1.xxxw, c69.xxxy
                min r1.xw, r1.xxxw, r10.xxxy
                add r1.xw, r1.xxxw, c68.xxxy
                rcp r8.x, c33.z
                rcp r8.y, c33.w
                mul r1.xw, r1.xxxw, r8.xxxy
                mov r2.xyzw, c69.xyzw
                mov r2.xy, r1.xwxx
                mov r8.xyzw, r2.xyzw
                texldl r8.xyzw, r8.xyzw, s8
                min r5.xyzw, r5.xyzw, r8.xyzw
                max r6.xyzw, r6.xyzw, r8.xyzw
                add r0.xy, r0.xyxx, c80.xyxx
                max r0.xy, r0.xyxx, c69.xyxx
                min r0.xy, r0.xyxx, r10.xyxx
                add r0.xy, r0.xyxx, c68.xyxx
                rcp r1.x, c33.z
                rcp r1.w, c33.w
                mul r0.xy, r0.xyxx, r1.xwxx
                mov r2.xyzw, c69.xyzw
                mov r2.xy, r0.xyxx
                texldl r2.xyzw, r2.xyzw, s8
                min r5.xyzw, r5.xyzw, r2.xyzw
                max r2.xyzw, r6.xyzw, r2.xyzw
                max r6.xyzw, r4.xyzw, r5.xyzw
                min r6.xyzw, r6.xyzw, r2.xyzw
                add r0.xy, r1.yzyy, -r0.zwzz
                dp2add r0.x, r0.xyxx, r0.xyxx, c69.x
                rsq r0.x, r0.x
                rcp r0.x, r0.x
                mov r0.x, -r0.x
                add r0.x, r0.x, c74.x
                mov_sat r0.x, r0.x
                max r0.y, c30.z, c72.x
                rcp r0.y, r0.y
                mul r0.y, r0.y, c71.y
                mul r0.y, r0.y, c30.y
                mul r0.x, r0.y, r0.x
                add r0.y, r5.w, -r0.x
                add r0.x, r2.w, r0.x
                max r0.y, r4.w, r0.y
                min r0.x, r0.y, r0.x
                add r0.y, r0.x, -r6.w
                abs r0.y, r0.y
                mul r0.y, r0.y, c30.z
                mul r0.y, r0.y, c15.w
                mov r1.xyzw, r3.xyzw
                add r3.xyz, r5.xyzx, -r0.y
                add r0.yzw, r2.xxyz, r0.y
                max r2.xyz, r4.xyzx, r3.xyzx
                min r0.yzw, r2.xxyz, r0.xyzw
                mov r2.xyz, r0.yzwy
                mov r2.w, r0.x
                mov r0.xyzw, r2.xyzw
                add r0.xyzw, r0.xyzw, -r1.xyzw
                mul r0.xyzw, c57.x, r0.xyzw
                add r0.xyzw, r1.xyzw, r0.xyzw
                mov r9.xyzw, r0.xyzw
                mov r11.xyzw, r7.xyzw
            else
            endif
        else
        endif
    else
    endif
else
endif
mov oC0.xyzw, r9.xyzw
mov oC1.xyzw, r11.xyzw
