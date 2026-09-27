ps_3_0
dcl_texcoord0 v0
def c68 = -1.00000000e+00, -1.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c69 = 5.00000000e-01, 5.00000000e-01, 9.99999975e-06, -9.99989986e-01
def c70 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c71 = 1.00000000e+00, 2.00000000e+00, -2.00000000e+00, 1.50000000e+00
def c72 = -1.00000000e+00, 1.00000000e+00, 1.00000000e+00, 1.00000000e+00
def c73 = 4.00000000e+01, 9.99999996e-13, 3.49999994e-01, 3.99999991e-02
def c74 = -8.50000024e-01, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
defi i0 = 255, 0, 0, 0
dcl_2d s1
mul r0.xy, v0.xyxx, c33.xyxx
frc r1.xyzw, r0.xyxx
add r0.xy, r0.xyxx, -r1.xyzw
add r0.zw, c33.xxxy, c68.xxxy
max r0.xy, r0.xyxx, c68.zwzz
min r0.xy, r0.xyxx, r0.zwzz
add r0.xy, r0.xyxx, c69.xyxx
mul r0.xy, r0.xyxx, c0.xyxx
mov r1.xyzw, c70.xyzw
mov r1.xy, r0.xyxx
texldl r1.xyzw, r1.xyzw, s1
add r1.x, r1.x, -c1.w
mul r1.x, r1.x, c2.x
mov_sat r1.x, r1.x
mul r1.y, c0.z, c0.w
add r1.z, c0.w, -c0.z
mul r1.w, r1.x, r1.z
add r1.w, c0.w, -r1.w
max r1.w, r1.w, c69.z
rcp r1.w, r1.w
mul r1.w, r1.y, r1.w
add r1.x, r1.x, c69.w
cmp r1.x, r1.x, c68.z, c71.x
add r1.x, -r1.x, c71.x
mov r2.xyzw, r3.xyzw
cmp r2.x, -r1.x, r2.x, c68.z
mov r3.x, r2.x
mov r2.xyzw, r3.xyzw
cmp r2.x, -r1.x, r2.y, c68.z
mov r3.y, r2.x
mov r2.xyzw, r3.xyzw
cmp r2.x, -r1.x, r2.z, c71.x
mov r3.z, r2.x
mov r2.xyzw, r3.xyzw
cmp r2.x, -r1.x, r2.w, r1.w
mov r3.w, r2.x
mov r2.xyzw, r3.xyzw
mov r3.xyzw, r4.xyzw
cmp r2.xyzw, -r1.x, r3.xyzw, r2.xyzw
mov r4.xyzw, r2.xyzw
add r1.x, -r1.x, c71.x
if_ne r1.x, -r1.x
    mul r2.xy, c0.xyxx, c69.xyxx
    add r2.zw, r0.xxxy, -r2.xxxy
    mul r2.zw, r2.xxzw, c71.xxyz
    add r2.zw, r2.xxzw, c72.xxxy
    rcp r3.x, c1.x
    rcp r3.y, c1.y
    mul r2.zw, r2.xxzw, r3.xxxy
    mov r3.xy, r2.zwzz
    mov r3.z, c1.z
    mov r5.xyz, r3.xyzx
    mul r5.xyz, r5.xyzx, r1.w
    add r2.zw, -r2.xxxy, c72.xxzw
    mul r1.x, c1.x, c71.w
    mul r1.x, r1.x, c33.x
    mul r1.x, r1.x, c69.x
    rcp r3.w, r1.w
    mul r1.x, r1.x, r3.w
    max r1.x, r1.x, c71.y
    min r1.x, r1.x, c73.x
    mov r3.w, c71.x
    mov r6.x, c68.z
    mov r6.y, c68.z
    mov r6.z, c68.z
    mov r7.x, c68.z
    mov r7.y, c68.z
    mov r7.z, c68.z
    mov r5.w, c68.z
    rep i0.xyzw
        mov r20.z, r5.w
        add r20.z, r20.z, c71.z
        cmp r20.z, r20.z, c68.z, c71.x
        add r20.z, -r20.z, c71.x
        if_ne r20.z, -r20.z
            break
        else
        endif
        mov r20.z, r5.w
        abs r20.z, r20.z
        add r20.z, -r20.z, -r20.z
        cmp r20.z, r20.z, c68.z, c71.x
        add r20.z, -r20.z, c71.x
        mov r13.x, c0.x
        mov r13.y, c68.z
        mov r22.xy, r13.xyxx
        mov r12.x, c68.z
        mov r12.y, c0.y
        mov r22.zw, r12.xxxy
        cmp r20.zw, -r20.z, r22.xxzw, r22.xxxy
        add r22.xy, r0.xyxx, -r20.zwzz
        max r22.xy, r22.xyxx, r2.xyxx
        add r22.zw, r0.xxxy, r20.xxzw
        min r22.zw, r22.xxzw, r2.xxzw
        mov r16.xyzw, c70.xyzw
        mov r16.xy, r22.xyxx
        mov r23.xyzw, r16.xyzw
        texldl r23.xyzw, r23.xyzw, s1
        add r23.x, r23.x, -c1.w
        mul r23.x, r23.x, c2.x
        mov_sat r23.x, r23.x
        mul r23.x, r23.x, r1.z
        add r23.x, c0.w, -r23.x
        max r23.x, r23.x, c69.z
        rcp r7.w, r23.x
        mul r23.x, r1.y, r7.w
        add r22.xy, r22.xyxx, -r2.xyxx
        mul r22.xy, r22.xyxx, c71.yzyy
        add r22.xy, r22.xyxx, c72.xyxx
        rcp r10.x, c1.x
        rcp r10.y, c1.y
        mul r22.xy, r22.xyxx, r10.xyxx
        mov r3.xy, r22.xyxx
        mov r3.z, c1.z
        mov r23.yzw, r3.xxyz
        mul r23.xyz, r23.yzwy, r23.x
        mov r9.xyzw, c70.xyzw
        mov r9.xy, r22.zwzz
        mov r24.xyzw, r9.xyzw
        texldl r24.xyzw, r24.xyzw, s1
        add r22.x, r24.x, -c1.w
        mul r22.x, r22.x, c2.x
        mov_sat r22.x, r22.x
        mul r22.x, r22.x, r1.z
        add r22.x, c0.w, -r22.x
        max r22.x, r22.x, c69.z
        rcp r6.w, r22.x
        mul r22.x, r1.y, r6.w
        add r22.yz, r22.xzwx, -r2.xxyx
        mul r22.yz, r22.xyzx, c71.xyzx
        add r22.yz, r22.xyzx, c72.xxyx
        rcp r8.x, c1.x
        rcp r8.y, c1.y
        mul r22.yz, r22.xyzx, r8.xxyx
        mov r3.xy, r22.yzyy
        mov r3.z, c1.z
        mov r22.yzw, r3.xxyz
        mul r22.xyz, r22.yzwy, r22.x
        mul r24.xy, -r20.zwzz, r1.x
        add r24.xy, r0.xyxx, r24.xyxx
        max r24.xy, r24.xyxx, r2.xyxx
        min r24.xy, r24.xyxx, r2.zwzz
        mul r24.xy, r24.xyxx, c33.xyxx
        frc r25.xyzw, r24.xyxx
        add r24.xy, r24.xyxx, -r25.xyzw
        max r24.xy, r24.xyxx, c68.zwzz
        min r24.xy, r24.xyxx, r0.zwzz
        add r24.xy, r24.xyxx, c69.xyxx
        mul r24.xy, r24.xyxx, c0.xyxx
        mov r11.xyzw, c70.xyzw
        mov r11.xy, r24.xyxx
        mov r25.xyzw, r11.xyzw
        texldl r25.xyzw, r25.xyzw, s1
        add r22.w, r25.x, -c1.w
        mul r22.w, r22.w, c2.x
        mov_sat r22.w, r22.w
        mul r22.w, r22.w, r1.z
        add r22.w, c0.w, -r22.w
        max r22.w, r22.w, c69.z
        rcp r14.w, r22.w
        mul r22.w, r1.y, r14.w
        add r24.zw, r24.xxxy, -r2.xxxy
        mul r24.zw, r24.xxzw, c71.xxyz
        add r24.zw, r24.xxzw, c72.xxxy
        rcp r8.z, c1.x
        rcp r8.w, c1.y
        mul r24.zw, r24.xxzw, r8.xxzw
        mov r3.xy, r24.zwzz
        mov r3.z, c1.z
        mov r25.xyz, r3.xyzx
        mul r25.xyz, r25.xyzx, r22.w
        add r26.xyz, r23.xyzx, -r5.xyzx
        add r27.xyz, r25.xyzx, -r5.xyzx
        dp3 r23.w, r26.xyzx, r27.xyzx
        dp3 r24.z, r26.xyzx, r26.xyzx
        dp3 r24.w, r27.xyzx, r27.xyzx
        mul r24.w, r24.z, r24.w
        max r24.w, r24.w, c73.y
        rsq r14.z, r24.w
        mul r23.w, r23.w, r14.z
        add r24.xy, r24.xyxx, r20.zwzz
        max r24.xy, r24.xyxx, r2.xyxx
        min r24.xy, r24.xyxx, r2.zwzz
        mov r19.xyzw, c70.xyzw
        mov r19.xy, r24.xyxx
        mov r27.xyzw, r19.xyzw
        texldl r27.xyzw, r27.xyzw, s1
        add r24.w, r27.x, -c1.w
        mul r24.w, r24.w, c2.x
        mov_sat r24.w, r24.w
        mul r24.w, r24.w, r1.z
        add r24.w, c0.w, -r24.w
        max r24.w, r24.w, c69.z
        rcp r12.w, r24.w
        mul r24.w, r1.y, r12.w
        add r24.xy, r24.xyxx, -r2.xyxx
        mul r24.xy, r24.xyxx, c71.yzyy
        add r24.xy, r24.xyxx, c72.xyxx
        rcp r13.z, c1.x
        rcp r13.w, c1.y
        mul r24.xy, r24.xyxx, r13.zwzz
        mov r3.xy, r24.xyxx
        mov r3.z, c1.z
        mov r27.xyz, r3.xyzx
        mul r24.xyw, r27.xyxz, r24.w
        add r24.xyw, r25.xyxz, -r24.xyxw
        dp3 r25.w, r26.xyzx, r24.xywx
        dp3 r24.x, r24.xywx, r24.xywx
        mul r24.x, r24.z, r24.x
        max r24.x, r24.x, c73.y
        rsq r14.y, r24.x
        mul r24.x, r25.w, r14.y
        mul r24.y, r1.w, c73.w
        max r24.y, r24.y, c73.z
        add r22.w, r22.w, -r1.w
        abs r22.w, r22.w
        add r22.w, r24.y, -r22.w
        cmp r22.w, r22.w, c68.z, c71.x
        min r23.w, r23.w, r24.x
        add r23.w, r23.w, c74.x
        cmp r23.w, r23.w, c68.z, c71.x
        max r22.w, r22.w, r23.w
        cmp r24.xzw, -r22.w, r25.xxyz, r23.xxyz
        mul r25.xy, r20.zwzz, r1.x
        add r25.xy, r0.xyxx, r25.xyxx
        max r25.xy, r25.xyxx, r2.xyxx
        min r25.xy, r25.xyxx, r2.zwzz
        mul r25.xy, r25.xyxx, c33.xyxx
        frc r26.xyzw, r25.xyxx
        add r25.xy, r25.xyxx, -r26.xyzw
        max r25.xy, r25.xyxx, c68.zwzz
        min r25.xy, r25.xyxx, r0.zwzz
        add r25.xy, r25.xyxx, c69.xyxx
        mul r25.xy, r25.xyxx, c0.xyxx
        mov r17.xyzw, c70.xyzw
        mov r17.xy, r25.xyxx
        mov r26.xyzw, r17.xyzw
        texldl r26.xyzw, r26.xyzw, s1
        add r22.w, r26.x, -c1.w
        mul r22.w, r22.w, c2.x
        mov_sat r22.w, r22.w
        mul r22.w, r22.w, r1.z
        add r22.w, c0.w, -r22.w
        max r22.w, r22.w, c69.z
        rcp r15.x, r22.w
        mul r22.w, r1.y, r15.x
        add r25.zw, r25.xxxy, -r2.xxxy
        mul r25.zw, r25.xxzw, c71.xxyz
        add r25.zw, r25.xxzw, c72.xxxy
        rcp r18.y, c1.x
        rcp r18.z, c1.y
        mul r25.zw, r25.xxzw, r18.xxyz
        mov r3.xy, r25.zwzz
        mov r3.z, c1.z
        mov r26.xyz, r3.xyzx
        mul r26.xyz, r26.xyzx, r22.w
        add r27.xyz, r22.xyzx, -r5.xyzx
        add r28.xyz, r26.xyzx, -r5.xyzx
        dp3 r23.w, r27.xyzx, r28.xyzx
        dp3 r25.z, r27.xyzx, r27.xyzx
        dp3 r25.w, r28.xyzx, r28.xyzx
        mul r25.w, r25.z, r25.w
        max r25.w, r25.w, c73.y
        rsq r18.w, r25.w
        mul r23.w, r23.w, r18.w
        add r20.zw, r25.xxxy, -r20.xxzw
        max r20.zw, r20.xxzw, r2.xxxy
        min r20.zw, r20.xxzw, r2.xxzw
        mov r21.xyzw, c70.xyzw
        mov r21.xy, r20.zwzz
        mov r28.xyzw, r21.xyzw
        texldl r28.xyzw, r28.xyzw, s1
        add r25.x, r28.x, -c1.w
        mul r25.x, r25.x, c2.x
        mov_sat r25.x, r25.x
        mul r25.x, r25.x, r1.z
        add r25.x, c0.w, -r25.x
        max r25.x, r25.x, c69.z
        rcp r20.y, r25.x
        mul r25.x, r1.y, r20.y
        add r20.zw, r20.xxzw, -r2.xxxy
        mul r20.zw, r20.xxzw, c71.xxyz
        add r20.zw, r20.xxzw, c72.xxxy
        rcp r10.z, c1.x
        rcp r10.w, c1.y
        mul r20.zw, r20.xxzw, r10.xxzw
        mov r3.xy, r20.zwzz
        mov r3.z, c1.z
        mov r28.xyz, r3.xyzx
        mul r25.xyw, r28.xyxz, r25.x
        add r25.xyw, r26.xyxz, -r25.xyxw
        dp3 r20.z, r27.xyzx, r25.xywx
        dp3 r20.w, r25.xywx, r25.xywx
        mul r20.w, r25.z, r20.w
        max r20.w, r20.w, c73.y
        rsq r14.x, r20.w
        mul r20.z, r20.z, r14.x
        add r20.w, r22.w, -r1.w
        abs r20.w, r20.w
        add r20.w, r24.y, -r20.w
        cmp r20.w, r20.w, c68.z, c71.x
        min r20.z, r23.w, r20.z
        add r20.z, r20.z, c74.x
        cmp r20.z, r20.z, c68.z, c71.x
        max r20.z, r20.w, r20.z
        cmp r25.xyw, -r20.z, r26.xyxz, r22.xyxz
        mov r20.z, r3.w
        mov r18.x, r20.z
        add r26.xyz, r5.xyzx, -r24.xzwx
        add r28.xyz, r25.xywx, -r5.xyzx
        dp3 r20.z, r26.xyzx, r28.xyzx
        dp3 r20.w, r26.xyzx, r26.xyzx
        dp3 r22.w, r28.xyzx, r28.xyzx
        mul r20.w, r20.w, r22.w
        max r20.w, r20.w, c73.y
        rsq r20.x, r20.w
        mul r20.z, r20.z, r20.x
        add r20.z, r20.z, c74.x
        cmp r20.z, r20.z, c68.z, c71.x
        add r20.z, -r20.z, c71.x
        add r24.xyz, r25.xywx, -r24.xzwx
        mov r25.xyw, r15.yzxw
        cmp r24.xyz, -r20.z, r25.xywx, r24.xyzx
        mov r15.yzw, r24.xxyz
        add r20.w, -r20.z, c71.x
        if_ne r20.w, -r20.w
            add r25.xyw, r5.xyxz, -r23.xyxz
            dp3 r20.w, r25.xywx, r27.xyzx
            dp3 r22.w, r25.xywx, r25.xywx
            mul r22.w, r22.w, r25.z
            max r22.w, r22.w, c73.y
            rsq r12.z, r22.w
            mul r20.w, r20.w, r12.z
            add r20.w, r20.w, c74.x
            cmp r20.w, r20.w, c68.z, c71.x
            add r20.w, -r20.w, c71.x
            add r26.xyz, r22.xyzx, -r23.xyzx
            cmp r24.xyz, -r20.w, r24.xyzx, r26.xyzx
            mov r15.yzw, r24.xxyz
            cmp r20.z, -r20.w, r20.z, c71.x
            add r20.z, -r20.z, c71.x
            if_ne r20.z, -r20.z
                mov r18.x, c68.z
                add r20.z, r22.z, -r5.z
                abs r20.z, r20.z
                add r20.w, r23.z, -r5.z
                abs r20.w, r20.w
                add r20.z, r20.z, -r20.w
                cmp r20.z, r20.z, c68.z, c71.x
                cmp r22.xyz, -r20.z, r25.xywx, r27.xyzx
                mov r15.yzw, r22.xxyz
            else
            endif
        else
        endif
        mov r20.z, r18.x
        mov r3.w, r20.z
        mov r22.xyz, r15.yzwy
        mov r20.z, r5.w
        abs r20.z, r20.z
        add r20.z, -r20.z, -r20.z
        cmp r20.z, r20.z, c68.z, c71.x
        add r20.z, -r20.z, c71.x
        mov r23.xyz, r6.xyzx
        cmp r23.xyz, -r20.z, r23.xyzx, r22.xyzx
        mov r6.xyz, r23.xyzx
        mov r23.xyz, r7.xyzx
        cmp r22.xyz, -r20.z, r22.xyzx, r23.xyzx
        mov r7.xyz, r22.xyzx
        mov r20.z, r5.w
        add r20.z, r20.z, c71.x
        mov r5.w, r20.z
    endrep
    mov r0.xyz, r6.xyzx
    mov r1.xyz, r7.xyzx
    mul r2.xyz, r0.zxyz, r1.yzxy
    mul r0.xyz, r0.yzxy, r1.zxyz
    add r0.xyz, r0.xyzx, -r2.xyzx
    dp3 r0.w, r0.xyzx, r0.xyzx
    max r0.w, r0.w, c73.y
    rsq r0.w, r0.w
    mov r1.xyz, r0.w
    mul r0.xyz, r0.xyzx, r1.xyzx
    dp3 r0.w, r0.xyzx, -r5.xyzx
    cmp r0.w, r0.w, c68.z, c71.x
    cmp r0.xyz, -r0.w, r0.xyzx, -r0.xyzx
    mul r1.xyz, r0.x, c3.xyzx
    mul r2.xyz, r0.y, c4.xyzx
    add r1.xyz, r1.xyzx, r2.xyzx
    mul r0.xyz, r0.z, c5.xyzx
    add r0.xyz, r1.xyzx, r0.xyzx
    mov r0.w, r3.w
    mov r1.xyz, r0.xyzx
    mov r1.w, r0.w
    mov r0.xyzw, r1.xyzw
    mov r4.xyzw, r0.xyzw
else
endif
mov oC0.xyzw, r4.xyzw
