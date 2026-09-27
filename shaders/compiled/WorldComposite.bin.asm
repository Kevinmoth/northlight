ps_3_0
dcl_texcoord0 v0
def c68 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c69 = -1.00000000e+00, -1.00000000e+00, 5.00000000e-01, 5.00000000e-01
def c70 = -5.00000000e-01, 1.00000000e+00, 9.99999975e-06, 1.20000001e-02
def c71 = 6.99999975e-04, 3.00000003e-03, -9.99989986e-01, 1.00000002e+20
def c72 = -5.00000000e-01, -5.00000000e-01, -4.00000000e+00, 2.00000000e+00
def c73 = 1.00000000e+00, 1.00000000e+00, 1.50000006e-01, 9.99999978e-03
def c74 = 1.44269502e+00, 2.00000003e-01, 1.99999996e-02, -1.99999996e-02
def c75 = 9.99999975e-05, 1.50000006e-01, 1.50000006e-01, 1.50000006e-01
def c76 = -4.49999988e-01, -4.49999988e-01, -4.49999988e-01, -2.00000000e+00
def c77 = 2.00000000e+00, -2.00000000e+00, -1.00000000e+00, 1.00000000e+00
def c78 = 3.00000000e+00, 9.99999996e-13, 9.99999997e-07, 6.39999986e-01
def c79 = 1.36000001e+00, 1.20000005e+00, -1.50000000e+00, 2.50000000e-01
def c80 = 1.00000000e+00, 1.00000000e+00, 1.00000000e+00, -3.00000000e+00
defi i0 = 255, 0, 0, 0
dcl_2d s12
dcl_2d s1
dcl_2d s9
dcl_2d s8
dcl_2d s0
dcl_2d s11
mov r0.xyzw, c68.xyzw
mov r0.xy, v0.xyxx
texldl r0.xyzw, r0.xyzw, s0
mul r1.xy, v0.xyxx, c33.xyxx
frc r2.xyzw, r1.xyxx
add r1.xy, r1.xyxx, -r2.xyzw
add r1.zw, c33.xxxy, c69.xxxy
max r1.xy, r1.xyxx, c68.xyxx
min r1.xy, r1.xyxx, r1.zwzz
add r1.xy, r1.xyxx, c69.zwzz
mul r1.xy, r1.xyxx, c0.xyxx
mov r2.xyzw, c68.xyzw
mov r2.xy, r1.xyxx
texldl r2.xyzw, r2.xyzw, s1
add r2.x, r2.x, -c1.w
mul r2.x, r2.x, c2.x
mov_sat r2.x, r2.x
mov r2.yzw, r0.xxyz
add r3.x, c30.x, c70.x
cmp r3.x, r3.x, c68.x, c70.y
mov r3.y, r3.z
cmp r3.y, -r3.x, r3.y, c68.x
mov r3.z, r3.y
add r3.y, -r3.x, c70.y
if_ne r3.y, -r3.y
    mov r4.xyzw, c68.xyzw
    mov r4.xy, r1.xyxx
    texldl r4.xyzw, r4.xyzw, s11
    add r3.w, c0.z, -r4.x
    cmp r3.w, r3.w, c68.x, c70.y
    mul r5.x, c0.z, c0.w
    add r5.y, c0.w, -c0.z
    mul r5.y, r2.x, r5.y
    add r5.y, c0.w, -r5.y
    max r5.y, r5.y, c70.z
    rcp r5.y, r5.y
    mul r5.x, r5.x, r5.y
    mul r5.y, r4.x, c71.x
    max r5.y, r5.y, c70.w
    add r5.x, r5.x, r5.y
    add r5.x, r5.x, -r4.x
    cmp r5.x, r5.x, c68.x, c70.y
    add r5.x, -r5.x, c70.y
    min r3.w, r3.w, r5.x
    add r5.x, -r4.y, c71.y
    cmp r5.x, r5.x, c68.x, c70.y
    min r3.w, r3.w, r5.x
    cmp r3.w, -r3.w, c68.x, r4.x
    mov r3.z, r3.w
else
endif
mov r3.w, r3.z
cmp r4.x, -r3.w, c68.x, c70.y
mul r4.y, c0.z, c0.w
add r4.z, c0.w, -c0.z
mul r4.w, r2.x, r4.z
add r4.w, c0.w, -r4.w
max r4.w, r4.w, c70.z
rcp r5.x, r4.w
mul r5.x, r4.y, r5.x
cmp r3.w, -r4.x, r5.x, r3.w
mul r5.x, r3.w, c1.z
mad r5.x, r5.x, c25.x, c25.y
max r5.x, r5.x, c68.x
log r5.x, r5.x
mul r5.x, c25.z, r5.x
exp r5.x, r5.x
mov_sat r5.x, r5.x
add r5.x, r5.x, c69.x
mad r5.x, c25.w, r5.x, c70.y
add r5.y, -r5.x, c70.y
mul r5.yzw, r5.y, c26.xxyz
min r5.yzw, r5.xyzw, r0.xxyz
add r2.x, r2.x, c71.z
cmp r2.x, r2.x, c68.x, c70.y
add r4.x, -r4.x, c70.y
min r6.x, r2.x, r4.x
mov r6.y, c68.x
mov r6.z, c68.x
mov r6.w, c68.x
mov r7.x, c68.x
mov r7.y, c68.x
mov r7.z, c68.x
mov r7.w, c68.x
mov r8.x, c68.x
mov r8.y, c71.w
mov r9.x, c68.x
mov r9.y, c68.x
mov r9.z, c68.x
mov r9.w, c68.x
mov r10.x, c68.x
mov r10.y, c68.x
mov r10.z, c68.x
mov r11.x, c68.x
mov r11.y, c68.x
mov r11.z, c68.x
mov r11.w, c68.x
mov r12.x, c68.x
mov r12.y, c68.x
mov r12.z, c68.x
mov r12.w, c70.y
mov r8.z, c68.x
mov r8.w, c71.w
mul r13.xy, v0.xyxx, c33.zwzz
add r13.xy, r13.xyxx, c72.xyxx
frc r14.xyzw, r13.xyxx
add r13.zw, r13.xxxy, -r14.xxxy
add r13.xy, r13.xyxx, -r13.zwzz
rcp r4.w, r4.w
mul r4.w, r4.y, r4.w
mov r10.w, c68.x
rep i0.xyzw
    mov r19.w, r10.w
    add r19.w, r19.w, c72.z
    cmp r19.w, r19.w, c68.x, c70.y
    add r19.w, -r19.w, c70.y
    if_ne r19.w, -r19.w
        break
    else
    endif
    mov r19.w, r10.w
    mul r19.w, r19.w, c69.z
    frc r22.xyzw, r19.w
    add r19.w, r19.w, -r22.x
    mov r22.x, r10.w
    mul r22.y, r19.w, c72.w
    add r22.x, r22.x, -r22.y
    mov r16.x, r22.x
    mov r16.y, r19.w
    mov r22.xy, r16.xyxx
    add r22.xy, r13.zwzz, r22.xyxx
    add r22.zw, c33.xxzw, c69.xxxy
    max r22.xy, r22.xyxx, c68.xyxx
    min r22.xy, r22.xyxx, r22.zwzz
    add r22.xy, r22.xyxx, c69.zwzz
    rcp r16.z, c33.z
    rcp r16.w, c33.w
    mul r22.xy, r22.xyxx, r16.zwzz
    mul r22.zw, r22.xxxy, c33.xxxy
    frc r23.xyzw, r22.zwzz
    add r22.zw, r22.xxzw, -r23.xxxy
    max r22.zw, r22.xxzw, c68.xxxy
    min r22.zw, r22.xxzw, r1.xxzw
    add r22.zw, r22.xxzw, c69.xxzw
    mul r22.zw, r22.xxzw, c0.xxxy
    mov r17.xyzw, c68.xyzw
    mov r17.xy, r22.zwzz
    mov r23.xyzw, r17.xyzw
    texldl r23.xyzw, r23.xyzw, s1
    add r19.w, r23.x, -c1.w
    mul r19.w, r19.w, c2.x
    mov_sat r19.w, r19.w
    add r23.xy, -r13.xyxx, c73.xyxx
    mov r23.zw, r16.xxxy
    add r24.xy, r13.xyxx, -r23.xyxx
    mul r23.zw, r23.xxzw, r24.xxxy
    add r23.xy, r23.xyxx, r23.zwzz
    if_ne r6.x, -r6.x
        mul r23.z, r19.w, r4.z
        add r23.z, c0.w, -r23.z
        max r23.z, r23.z, c70.z
        rcp r14.w, r23.z
        mul r23.z, r4.y, r14.w
        add r23.z, r23.z, -r4.w
        abs r23.z, r23.z
        mul r23.w, r23.x, r23.y
        mul r24.x, r4.w, c73.w
        max r24.x, r24.x, c73.z
        rcp r19.x, r24.x
        mul r24.x, -r23.z, r19.x
        mul r24.x, r24.x, c74.x
        exp r19.y, r24.x
        mul r23.w, r23.w, r19.y
        mov r20.xyzw, c68.xyzw
        mov r20.xy, r22.xyxx
        mov r24.xyzw, r20.xyzw
        texldl r24.xyzw, r24.xyzw, s8
        mov r21.xyzw, c68.xyzw
        mov r21.xy, r22.xyxx
        mov r25.xyzw, r21.xyzw
        texldl r25.xyzw, r25.xyzw, s12
        mul r26.xyz, r24.xyzx, r23.w
        mov r27.xyz, r7.xyzx
        add r26.xyz, r27.xyzx, r26.xyzx
        mov r7.xyz, r26.xyzx
        mul r26.x, r24.w, r23.w
        mov r26.y, r7.w
        add r26.x, r26.y, r26.x
        mov r7.w, r26.x
        mul r26.xyz, r25.xyzx, r23.w
        mov r27.xyz, r6.yzwy
        add r26.xyz, r27.xyzx, r26.xyzx
        mov r6.yzw, r26.xxyz
        mov r26.x, r8.x
        add r23.w, r26.x, r23.w
        mov r8.x, r23.w
        mov r23.w, r8.y
        add r23.w, r23.z, -r23.w
        cmp r23.w, r23.w, c68.x, c70.y
        mov r26.x, r8.y
        cmp r23.z, -r23.w, r26.x, r23.z
        mov r8.y, r23.z
        mov r26.xyzw, r9.xyzw
        cmp r24.xyzw, -r23.w, r26.xyzw, r24.xyzw
        mov r9.xyzw, r24.xyzw
        mov r24.xyz, r10.xyzx
        cmp r24.xyz, -r23.w, r24.xyzx, r25.xyzx
        mov r10.xyz, r24.xyzx
    else
    endif
    mov r23.z, r3.z
    cmp r23.z, -r3.x, r23.z, c68.x
    mov r3.z, r23.z
    if_ne r3.y, -r3.y
        mov r18.xyzw, c68.xyzw
        mov r18.xy, r22.zwzz
        mov r24.xyzw, r18.xyzw
        texldl r24.xyzw, r24.xyzw, s11
        add r22.z, c0.z, -r24.x
        cmp r22.z, r22.z, c68.x, c70.y
        mul r22.w, r19.w, r4.z
        add r22.w, c0.w, -r22.w
        max r22.w, r22.w, c70.z
        rcp r14.y, r22.w
        mul r22.w, r4.y, r14.y
        mul r23.z, r24.x, c71.x
        max r23.z, r23.z, c70.w
        add r22.w, r22.w, r23.z
        add r22.w, r22.w, -r24.x
        cmp r22.w, r22.w, c68.x, c70.y
        add r22.w, -r22.w, c70.y
        min r22.z, r22.z, r22.w
        add r22.w, -r24.y, c71.y
        cmp r22.w, r22.w, c68.x, c70.y
        min r22.z, r22.z, r22.w
        cmp r22.z, -r22.z, c68.x, r24.x
        mov r3.z, r22.z
    else
    endif
    mov r22.z, r3.z
    cmp r22.w, -r22.z, c68.x, c70.y
    mul r19.w, r19.w, r4.z
    add r19.w, c0.w, -r19.w
    max r19.w, r19.w, c70.z
    rcp r14.x, r19.w
    mul r19.w, r4.y, r14.x
    cmp r19.w, -r22.w, r19.w, r22.z
    add r19.w, r19.w, -r3.w
    abs r19.w, r19.w
    mov r15.xyzw, c68.xyzw
    mov r15.xy, r22.xyxx
    mov r22.xyzw, r15.xyzw
    texldl r22.xyzw, r22.xyzw, s9
    mul r23.x, r23.x, r23.y
    mul r23.y, r3.w, c74.z
    max r23.y, r23.y, c74.y
    rcp r14.z, r23.y
    mul r23.y, -r19.w, r14.z
    mul r23.y, r23.y, c74.x
    exp r19.z, r23.y
    mul r23.x, r23.x, r19.z
    mul r24.xyzw, r22.xyzw, r23.x
    mov r25.xyzw, r11.xyzw
    add r24.xyzw, r25.xyzw, r24.xyzw
    mov r11.xyzw, r24.xyzw
    mov r23.y, r8.z
    add r23.x, r23.y, r23.x
    mov r8.z, r23.x
    mov r23.x, r8.w
    add r23.x, r19.w, -r23.x
    cmp r23.x, r23.x, c68.x, c70.y
    mov r23.y, r8.w
    cmp r19.w, -r23.x, r23.y, r19.w
    mov r8.w, r19.w
    mov r24.xyzw, r12.xyzw
    cmp r22.xyzw, -r23.x, r24.xyzw, r22.xyzw
    mov r12.xyzw, r22.xyzw
    mov r19.w, r10.w
    add r19.w, r19.w, c70.y
    mov r10.w, r19.w
endrep
mov r1.z, r8.z
add r1.z, -r1.z, c70.z
cmp r1.z, r1.z, c68.x, c70.y
mov r1.w, r8.z
rcp r1.w, r1.w
mov r13.xyzw, r1.w
mul r11.xyzw, r11.xyzw, r13.xyzw
cmp r11.xyzw, -r1.z, r12.xyzw, r11.xyzw
if_ne r6.x, -r6.x
    mov r1.z, r8.x
    add r1.z, r1.z, c74.w
    cmp r1.z, r1.z, c68.x, c70.y
    mov r12.xyzw, r9.xyzw
    mov r3.xyz, r7.xyzx
    cmp r3.xyz, -r1.z, r3.xyzx, r12.xyzx
    mov r1.w, r7.w
    cmp r1.w, -r1.z, r1.w, r9.w
    mov r4.yzw, r10.xxyz
    mov r6.xyz, r6.yzwy
    cmp r4.yzw, -r1.z, r6.xxyz, r4.xyzw
    mov r6.x, r8.x
    cmp r1.z, -r1.z, r6.x, c70.y
    max r1.z, r1.z, c75.x
    rcp r1.z, r1.z
    mov r6.xyz, r1.z
    mul r3.xyz, r3.xyzx, r6.xyzx
    mul r1.w, r1.w, r1.z
    mov r6.xyz, r1.z
    mul r4.yzw, r4.xyzw, r6.xxyz
    max r4.yzw, r4.xyzw, c75.xyzw
    add r5.yzw, r0.xxyz, -r5.xyzw
    max r5.yzw, r5.xyzw, c68.xxyz
    rcp r6.x, r4.y
    rcp r6.y, r4.z
    rcp r6.z, r4.w
    mul r4.yzw, r5.xyzw, r6.xxyz
    min r4.yzw, r4.xyzw, r5.x
    mad r4.yzw, r4.xyzw, r3.xxyz, r0.xxyz
    mov r2.yzw, r4.xyzw
    mad r5.xyz, c76.xyzx, r5.yzwy, r0.xyzx
    max r4.yzw, r4.xyzw, r5.xxyz
    mov r2.yzw, r4.xyzw
    add r1.z, c24.z, c69.x
    abs r1.z, r1.z
    add r1.z, -r1.z, -r1.z
    cmp r1.z, r1.z, c68.x, c70.y
    add r1.z, -r1.z, c70.y
    cmp r4.yzw, -r1.z, r4.xyzw, r1.w
    mov r2.yzw, r4.xyzw
    add r1.z, c24.z, c76.w
    abs r1.z, r1.z
    add r1.z, -r1.z, -r1.z
    cmp r1.z, r1.z, c68.x, c70.y
    add r1.z, -r1.z, c70.y
    add r3.xyz, c18.xyzx, r3.xyzx
    max r3.xyz, r3.xyzx, c68.xyzx
    cmp r3.xyz, -r1.z, r4.yzwy, r3.xyzx
    mov r2.yzw, r3.xxyz
else
endif
add r1.z, c24.z, c70.x
cmp r1.z, r1.z, c68.x, c70.y
if_ne r1.z, -r1.z
    mov r3.xyz, r2.yzwy
    add r1.z, -r2.x, c70.y
    min r1.z, r1.z, r4.x
    mov r1.w, c68.x
    add r2.x, r3.w, -c57.y
    mul r2.x, r2.x, c57.z
    mov_sat r2.x, r2.x
    cmp r1.z, -r1.z, r2.x, c70.y
    cmp r2.x, -r1.z, c68.x, c70.y
    add r2.x, -r2.x, c70.y
    cmp r3.w, -c34.w, c68.x, c70.y
    add r3.w, -r3.w, c70.y
    max r2.x, r2.x, r3.w
    if_ne r2.x, -r2.x
        mov r4.xyz, r3.xyzx
        mov r1.w, c70.y
    else
    endif
    add r1.w, -r1.w, c70.y
    if_ne r1.w, -r1.w
        mul r5.xy, c0.xyxx, c69.zwzz
        add r1.xy, r1.xyxx, -r5.xyxx
        mul r1.xy, r1.xyxx, c77.xyxx
        add r1.xy, r1.xyxx, c77.zwzz
        rcp r5.x, c1.x
        rcp r5.y, c1.y
        mul r1.xy, r1.xyxx, r5.xyxx
        mov r5.xy, r1.xyxx
        mov r5.z, c1.z
        mul r6.xyz, r1.x, c3.xyzx
        mul r1.xyw, r1.y, c4.xyxz
        add r1.xyw, r6.xyxz, r1.xyxw
        mul r6.xyz, c1.z, c5.xyzx
        add r1.xyw, r1.xyxw, r6.xyxz
        mov r6.xyz, r5.xyzx
        dp3 r2.x, r6.xyzx, r5.xyzx
        rsq r2.x, r2.x
        mul r2.x, r1.w, r2.x
        max r2.x, r2.x, c68.x
        mul r2.x, -r2.x, c57.w
        exp r2.x, r2.x
        mul r3.w, r1.z, r1.z
        mul r1.z, r1.z, c72.w
        add r1.z, -r1.z, c78.x
        mul r1.z, r3.w, r1.z
        mul r2.x, -c34.w, r2.x
        exp r2.x, r2.x
        mov r2.x, -r2.x
        add r2.x, r2.x, c70.y
        mul r1.z, r1.z, r2.x
        dp2add r2.x, c67.zwzz, c67.zwzz, c68.x
        rsq r2.x, r2.x
        rcp r2.x, r2.x
        dp2add r3.w, r1.xyxx, c67.zwzz, c68.x
        dp2add r1.x, r1.xyxx, r1.xyxx, c68.x
        max r1.x, r1.x, c78.y
        rsq r1.x, r1.x
        mul r1.x, r3.w, r1.x
        max r1.y, r2.x, c78.z
        rcp r1.y, r1.y
        mul r1.x, r1.x, r1.y
        mul r1.y, r2.x, c78.w
        mul r1.x, r1.x, c79.y
        add r1.x, -r1.x, c79.x
        log r1.x, r1.x
        mul r1.x, r1.x, c79.z
        exp r1.x, r1.x
        mul r1.x, r1.y, r1.x
        min r1.x, r1.x, c79.w
        mad r1.xyw, r1.x, c35.yzxw, c80.xyxz
        mul r1.xyw, c34.xyxz, r1.xyxw
        add r1.xyw, r1.xyxw, -r3.xyxz
        mul r1.xyz, r1.z, r1.xywx
        add r1.xyz, r3.xyzx, r1.xyzx
        mov r4.xyz, r1.xyzx
    else
    endif
    mov r1.xyz, r4.xyzx
    mov r2.yzw, r1.xxyz
    mad r1.xyz, r1.xyzx, r11.w, r11.xyzx
    mov r2.yzw, r1.xxyz
else
endif
add r1.x, c24.z, c80.w
abs r1.x, r1.x
add r1.x, -r1.x, -r1.x
cmp r1.x, r1.x, c68.x, c70.y
add r1.x, -r1.x, c70.y
mov r1.yzw, r2.xyzw
cmp r1.xyz, -r1.x, r1.yzwy, r11.xyzx
max r1.xyz, r1.xyzx, c68.xyzx
mov r1.w, r0.w
mov oC0.xyzw, r1.xyzw
