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
            mov r15.w, r10.w
            add r15.w, r15.w, c75.x
            cmp r15.w, r15.w, c69.x, c70.y
            add r15.w, -r15.w, c70.y
            if_ne r15.w, -r15.w
                break
            else
            endif
            mov r16.zw, r11.xxxy
            mov r15.w, r9.w
            mul r15.w, r15.w, c75.y
            rsq r14.x, r15.w
            rcp r15.z, r14.x
            mul r15.w, r15.z, r1.x
            mul r16.zw, r16.xxzw, r15.w
            add r16.zw, r0.xxzw, r16.xxzw
            frc r19.xyzw, r16.zwzz
            add r16.zw, r16.xxzw, -r19.xxxy
            add r19.xy, c33.zwzz, c68.zwzz
            max r16.zw, r16.xxzw, c69.xxxy
            min r16.zw, r16.xxzw, r19.xxxy
            add r16.zw, r16.xxzw, c68.xxxy
            rcp r16.x, c33.z
            rcp r16.y, c33.w
            mul r16.zw, r16.xxzw, r16.xxxy
            mov r17.xyzw, c69.xyzw
            mov r17.xy, r16.zwzz
            mov r19.xyzw, r17.xyzw
            texldl r19.xyzw, r19.xyzw, s8
            mul r20.xy, r16.zwzz, c33.xyxx
            frc r21.xyzw, r20.xyxx
            add r20.xy, r20.xyxx, -r21.xyzw
            max r20.xy, r20.xyxx, c69.xyxx
            min r20.xy, r20.xyxx, r4.xyxx
            add r20.xy, r20.xyxx, c68.xyxx
            mul r20.xy, r20.xyxx, c0.xyxx
            mov r18.xyzw, c69.xyzw
            mov r18.xy, r20.xyxx
            mov r20.xyzw, r18.xyzw
            texldl r20.xyzw, r20.xyzw, s1
            add r15.w, r20.x, -c1.w
            mul r15.w, r15.w, c2.x
            mov_sat r15.w, r15.w
            mul r15.w, r15.w, r5.x
            add r15.w, c0.w, -r15.w
            max r15.w, r15.w, c70.x
            rcp r11.w, r15.w
            mul r15.w, r4.w, r11.w
            add r15.w, r15.w, -r5.z
            abs r15.w, r15.w
            mul r15.w, r15.w, r1.y
            exp r11.z, r15.w
            add r15.w, r19.w, -r2.w
            abs r15.w, r15.w
            mul r15.w, r15.w, r7.w
            add r15.w, -r15.w, c70.y
            mov_sat r15.w, r15.w
            mul r15.w, r11.z, r15.w
            mul r19.xyz, r19.xyzx, r6.w
            mov r12.xyzw, c69.xyzw
            mov r12.xy, r16.zwzz
            mov r20.xyzw, r12.xyzw
            texldl r20.xyzw, r20.xyzw, s12
            mov r13.xyzw, c69.xyzw
            mov r13.xy, r16.zwzz
            mov r21.xyzw, r13.xyzw
            texldl r21.xyzw, r21.xyzw, s0
            max r20.xyz, r20.xyzx, c71.yzwy
            mul r20.xyz, r6.w, r20.xyzx
            min r22.xyz, r7.xyzx, r21.xyzx
            add r21.xyz, r21.xyzx, -r22.xyzx
            max r20.xyz, r20.xyzx, r21.xyzx
            max r20.xyz, r20.xyzx, c72.xyzx
            rcp r14.y, r20.x
            rcp r14.z, r20.y
            rcp r14.w, r20.z
            mul r19.xyz, r19.xyzx, r14.yzwy
            max r19.xyz, r19.xyzx, c74.yzwy
            mul r19.xyz, r19.xyzx, r15.w
            mov r20.xyz, r10.xyzx
            add r19.xyz, r20.xyzx, r19.xyzx
            mov r10.xyz, r19.xyzx
            mov r16.z, r8.w
            add r15.w, r16.z, r15.w
            mov r8.w, r15.w
            mov r16.zw, r11.xxxy
            mul r15.w, r16.z, c75.z
            mov r16.zw, r11.xxxy
            mul r16.z, r16.w, c75.w
            add r15.w, r15.w, -r16.z
            mov r16.zw, r11.xxxy
            mul r16.z, r16.z, c75.w
            mov r19.xy, r11.xyxx
            mul r16.w, r19.y, c76.x
            add r16.z, r16.z, -r16.w
            mov r15.x, r15.w
            mov r15.y, r16.z
            mov r16.zw, r15.xxxy
            mov r11.xy, r16.zwzz
            mov r15.w, r9.w
            add r15.w, r15.w, c70.y
            mov r9.w, r15.w
            mov r15.w, r10.w
            add r15.w, r15.w, c70.y
            mov r10.w, r15.w
        endrep
        mov r7.xyz, r10.xyzx
        mov r0.z, r8.w
        rcp r10.x, r0.z
        rcp r10.y, r0.z
        rcp r10.z, r0.z
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
cmp r0.z, -c57.x, c69.x, c70.y
add r0.z, -r0.z, c70.y
add r0.w, -r4.z, c70.y
max r0.z, r0.z, r0.w
mov r8.xyzw, r9.xyzw
cmp r8.xyzw, -r0.z, r8.xyzw, r6.xyzw
mov r9.xyzw, r8.xyzw
mov r10.xyzw, r11.xyzw
cmp r10.xyzw, -r0.z, r10.xyzw, r7.xyzw
mov r11.xyzw, r10.xyzw
add r0.w, -r0.z, c70.y
if_ne r0.w, -r0.w
    rcp r0.w, r5.y
    mul r0.w, r4.w, r0.w
    mul r1.xy, c0.xyxx, c68.xyxx
    add r1.xy, r1.zwzz, -r1.xyxx
    mul r1.xy, r1.xyxx, c76.yzyy
    add r1.xy, r1.xyxx, c77.xyxx
    rcp r1.z, c1.x
    rcp r1.w, c1.y
    mul r1.xy, r1.xyxx, r1.zwzz
    mov r1.z, c1.z
    mul r1.xyz, r1.xyzx, r0.w
    mul r4.xyz, r1.x, c3.xyzx
    mul r5.xyz, r1.y, c4.xyzx
    add r4.xyz, r4.xyzx, r5.xyzx
    mul r1.xyz, r1.z, c5.xyzx
    add r1.xyz, r4.xyzx, r1.xyzx
    add r1.xyz, r1.xyzx, c6.xyzx
    mul r4.xyz, r1.x, c53.xyzx
    mul r5.xyz, r1.y, c54.xyzx
    add r4.xyz, r4.xyzx, r5.xyzx
    mul r1.xyz, r1.z, c55.xyzx
    add r1.xyz, r4.xyzx, r1.xyzx
    add r1.xyz, r1.xyzx, c56.xyzx
    mul r0.w, r1.z, c1.z
    add r1.w, c0.z, -r0.w
    cmp r1.w, r1.w, c69.x, c70.y
    add r1.w, -r1.w, c70.y
    cmp r4.xyzw, -r1.w, r8.xyzw, r6.xyzw
    mov r9.xyzw, r4.xyzw
    cmp r5.xyzw, -r1.w, r10.xyzw, r7.xyzw
    mov r11.xyzw, r5.xyzw
    cmp r0.z, -r1.w, r0.z, c70.y
    add r1.w, -r0.z, c70.y
    if_ne r1.w, -r1.w
        mul r1.xy, r1.xyxx, c1.xyxx
        rcp r1.z, r0.w
        rcp r1.w, r0.w
        mul r1.xy, r1.xyxx, r1.zwzz
        mul r1.xy, r1.xyxx, c77.zwzz
        add r1.xy, r1.xyxx, c68.xyxx
        cmp r1.zw, r1.xxxy, c69.xxxy, c78.xxxy
        max r1.z, r1.z, r1.w
        add r8.xy, -r1.xyxx, c78.xyxx
        cmp r8.xy, r8.xyxx, c69.xyxx, c78.xyxx
        max r1.w, r8.x, r8.y
        max r1.z, r1.z, r1.w
        cmp r4.xyzw, -r1.z, r4.xyzw, r6.xyzw
        mov r9.xyzw, r4.xyzw
        cmp r5.xyzw, -r1.z, r5.xyzw, r7.xyzw
        mov r11.xyzw, r5.xyzw
        cmp r0.z, -r1.z, r0.z, c70.y
        add r1.z, -r0.z, c70.y
        if_ne r1.z, -r1.z
            mul r1.xy, r1.xyxx, c33.zwzz
            frc r8.xyzw, r1.xyxx
            add r1.xy, r1.xyxx, -r8.xyzw
            add r1.zw, c33.xxzw, c68.xxzw
            max r1.xy, r1.xyxx, c69.xyxx
            min r1.xy, r1.xyxx, r1.zwzz
            add r1.xy, r1.xyxx, c68.xyxx
            rcp r8.x, c33.z
            rcp r8.y, c33.w
            mul r1.xy, r1.xyxx, r8.xyxx
            mov r8.xyzw, c69.xyzw
            mov r8.xy, r1.xyxx
            texldl r8.xyzw, r8.xyzw, s15
            mul r10.x, r0.w, c73.w
            max r10.x, r10.x, c73.z
            add r0.w, r8.x, -r0.w
            abs r0.w, r0.w
            add r0.w, r10.x, -r0.w
            cmp r0.w, r0.w, c69.x, c70.y
            cmp r4.xyzw, -r0.w, r4.xyzw, r6.xyzw
            mov r9.xyzw, r4.xyzw
            cmp r4.xyzw, -r0.w, r5.xyzw, r7.xyzw
            mov r11.xyzw, r4.xyzw
            cmp r0.z, -r0.w, r0.z, c70.y
            add r0.z, -r0.z, c70.y
            if_ne r0.z, -r0.z
                mov r4.xyzw, c69.xyzw
                mov r4.xy, r1.xyxx
                texldl r4.xyzw, r4.xyzw, s14
                mov r5.xyzw, r3.xyzw
                mov r5.w, r2.w
                mov r6.xyzw, r3.xyzw
                mov r6.w, r2.w
                add r0.zw, r0.xxxy, c78.xxzw
                max r0.zw, r0.xxzw, c69.xxxy
                min r0.zw, r0.xxzw, r1.xxzw
                add r0.zw, r0.xxzw, c68.xxxy
                rcp r1.x, c33.z
                rcp r1.y, c33.w
                mul r0.zw, r0.xxzw, r1.xxxy
                mov r2.xyzw, c69.xyzw
                mov r2.xy, r0.zwzz
                mov r8.xyzw, r2.xyzw
                texldl r8.xyzw, r8.xyzw, s8
                min r5.xyzw, r5.xyzw, r8.xyzw
                max r6.xyzw, r6.xyzw, r8.xyzw
                add r0.zw, r0.xxxy, c79.xxxy
                max r0.zw, r0.xxzw, c69.xxxy
                min r0.zw, r0.xxzw, r1.xxzw
                add r0.zw, r0.xxzw, c68.xxxy
                rcp r1.x, c33.z
                rcp r1.y, c33.w
                mul r0.zw, r0.xxzw, r1.xxxy
                mov r2.xyzw, c69.xyzw
                mov r2.xy, r0.zwzz
                mov r8.xyzw, r2.xyzw
                texldl r8.xyzw, r8.xyzw, s8
                min r5.xyzw, r5.xyzw, r8.xyzw
                max r6.xyzw, r6.xyzw, r8.xyzw
                add r0.zw, r0.xxxy, c79.xxzw
                max r0.zw, r0.xxzw, c69.xxxy
                min r0.zw, r0.xxzw, r1.xxzw
                add r0.zw, r0.xxzw, c68.xxxy
                rcp r1.x, c33.z
                rcp r1.y, c33.w
                mul r0.zw, r0.xxzw, r1.xxxy
                mov r2.xyzw, c69.xyzw
                mov r2.xy, r0.zwzz
                mov r8.xyzw, r2.xyzw
                texldl r8.xyzw, r8.xyzw, s8
                min r5.xyzw, r5.xyzw, r8.xyzw
                max r6.xyzw, r6.xyzw, r8.xyzw
                add r0.xy, r0.xyxx, c80.xyxx
                max r0.xy, r0.xyxx, c69.xyxx
                min r0.xy, r0.xyxx, r1.zwzz
                add r0.xy, r0.xyxx, c68.xyxx
                rcp r0.z, c33.z
                rcp r0.w, c33.w
                mul r0.xy, r0.xyxx, r0.zwzz
                mov r2.xyzw, c69.xyzw
                mov r2.xy, r0.xyxx
                mov r0.xyzw, r2.xyzw
                texldl r0.xyzw, r0.xyzw, s8
                min r1.xyzw, r5.xyzw, r0.xyzw
                max r0.xyzw, r6.xyzw, r0.xyzw
                mov r2.xyzw, r3.xyzw
                max r1.xyzw, r4.xyzw, r1.xyzw
                min r0.xyzw, r1.xyzw, r0.xyzw
                add r0.xyzw, r0.xyzw, -r2.xyzw
                mul r0.xyzw, c57.x, r0.xyzw
                add r0.xyzw, r2.xyzw, r0.xyzw
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
