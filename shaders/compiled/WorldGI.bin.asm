ps_3_0
dcl_texcoord0 v0
def c68 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c69 = -1.00000000e+00, -1.00000000e+00, 5.00000000e-01, 5.00000000e-01
def c70 = -9.99989986e-01, 1.00000000e+00, -5.00000000e-01, 9.99999975e-06
def c71 = 1.20000001e-02, 6.99999975e-04, 3.00000003e-03, 4.88602519e-01
def c72 = 2.00000000e+00, -2.00000000e+00, -1.00000000e+00, 1.00000000e+00
def c73 = 2.50000000e-01, 2.50000000e-01, 2.50000000e-01, 2.82094777e-01
def c74 = 3.14159274e+00, 2.09439516e+00, 2.09439516e+00, 2.09439516e+00
def c75 = -8.00000000e+00, 1.00000000e+00, 1.00000000e+00, 1.00000000e+00
def c76 = 2.22222233e+00, 3.00000000e+00, 4.00000000e+00, 5.00000000e+00
def c77 = 6.00000000e+00, 1.99999996e-02, 0.00000000e+00, 0.00000000e+00
def c78 = 3.18309873e-01, 3.18309873e-01, 3.18309873e-01, 0.00000000e+00
defi i0 = 255, 0, 0, 0
dcl_2d s1
dcl_2d s14
dcl_2d s6
dcl_2d s5
dcl_2d s10
dcl_2d s4
dcl_2d s7
dcl_2d s11
mov r0.xyzw, c68.xyzw
mov r0.xy, v0.xyxx
texldl r0.xyzw, r0.xyzw, s14
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
add r1.z, r2.x, -c1.w
mul r1.z, r1.z, c2.x
mov_sat r1.z, r1.z
add r1.w, r1.z, c70.x
cmp r1.w, r1.w, c68.x, c70.y
add r1.w, -r1.w, c70.y
add r2.x, c30.x, c70.z
cmp r2.x, r2.x, c68.x, c70.y
mov r2.y, r2.z
cmp r2.y, -r2.x, r2.y, c68.x
mov r2.z, r2.y
add r2.x, -r2.x, c70.y
if_ne r2.x, -r2.x
    mov r3.xyzw, c68.xyzw
    mov r3.xy, r1.xyxx
    texldl r3.xyzw, r3.xyzw, s11
    add r2.x, c0.z, -r3.x
    cmp r2.x, r2.x, c68.x, c70.y
    mul r2.y, c0.z, c0.w
    add r2.w, c0.w, -c0.z
    mul r2.w, r1.z, r2.w
    add r2.w, c0.w, -r2.w
    max r2.w, r2.w, c70.w
    rcp r2.w, r2.w
    mul r2.y, r2.y, r2.w
    mul r2.w, r3.x, c71.y
    max r2.w, r2.w, c71.x
    add r2.y, r2.y, r2.w
    add r2.y, r2.y, -r3.x
    cmp r2.y, r2.y, c68.x, c70.y
    add r2.y, -r2.y, c70.y
    min r2.x, r2.x, r2.y
    add r2.y, -r3.y, c71.z
    cmp r2.y, r2.y, c68.x, c70.y
    min r2.x, r2.x, r2.y
    cmp r2.x, -r2.x, c68.x, r3.x
    mov r2.z, r2.x
else
endif
mov r2.x, r2.z
cmp r2.x, -r2.x, c68.x, c70.y
max r1.w, r1.w, r2.x
mov r2.xyzw, r3.xyzw
cmp r2.xyzw, -r1.w, r2.xyzw, c68.xyzw
mov r3.xyzw, r2.xyzw
add r1.w, -r1.w, c70.y
if_ne r1.w, -r1.w
    mul r1.w, c0.z, c0.w
    add r2.x, c0.w, -c0.z
    mul r1.z, r1.z, r2.x
    add r1.z, c0.w, -r1.z
    max r1.z, r1.z, c70.w
    rcp r1.z, r1.z
    mul r1.z, r1.w, r1.z
    mul r2.xy, c0.xyxx, c69.zwzz
    add r1.xy, r1.xyxx, -r2.xyxx
    mul r1.xy, r1.xyxx, c72.xyxx
    add r1.xy, r1.xyxx, c72.zwzz
    rcp r2.x, c1.x
    rcp r2.y, c1.y
    mul r1.xy, r1.xyxx, r2.xyxx
    mov r1.w, c1.z
    mul r1.xyz, r1.xywx, r1.z
    mul r2.xyz, r1.x, c3.xyzx
    mul r4.xyz, r1.y, c4.xyzx
    add r2.xyz, r2.xyzx, r4.xyzx
    mul r1.xyz, r1.z, c5.xyzx
    add r1.xyz, r2.xyzx, r1.xyzx
    add r1.xyz, r1.xyzx, c6.xyzx
    mul r2.xyz, r0.xyzx, c73.xyzx
    add r1.xyz, r1.xyzx, r2.xyzx
    add r1.w, c20.w, c70.z
    cmp r1.w, r1.w, c68.x, c70.y
    mov r2.xyzw, r4.xyzw
    cmp r2.xyzw, -r1.w, r2.xyzw, c68.xyzw
    mov r4.xyzw, r2.xyzw
    add r1.w, -r1.w, c70.y
    if_ne r1.w, -r1.w
        rcp r2.x, c19.w
        rcp r2.y, c19.w
        rcp r2.z, c19.w
        mul r2.xyz, r1.xyzx, r2.xyzx
        frc r5.xyzw, r2.xyzx
        add r5.xyz, r2.xyzx, -r5.xyzw
        add r2.xyz, r2.xyzx, -r5.xyzx
        mul r1.w, r0.x, c71.w
        mul r2.w, r0.y, c71.w
        mul r5.w, r0.z, c71.w
        mov r6.x, c73.w
        mov r6.y, r1.w
        mov r6.z, r2.w
        mov r6.w, r5.w
        mul r6.xyzw, r6.xyzw, c74.xyzw
        mov r7.x, c68.x
        mov r7.y, c68.x
        mov r7.z, c68.x
        mov r1.w, c68.x
        mov r2.w, c68.x
        mov r5.w, c68.x
        rep i0.xyzw
            mov r15.w, r5.w
            add r15.w, r15.w, c75.x
            cmp r15.w, r15.w, c68.x, c70.y
            add r15.w, -r15.w, c70.y
            if_ne r15.w, -r15.w
                break
            else
            endif
            mov r15.w, r5.w
            mov r19.x, r5.w
            mul r19.x, r19.x, c69.z
            frc r20.xyzw, r19.x
            add r19.x, r19.x, -r20.xyzw
            mul r19.x, r19.x, c72.x
            add r15.w, r15.w, -r19.x
            mov r19.x, r5.w
            mul r19.x, r19.x, c69.z
            frc r20.xyzw, r19.x
            add r19.x, r19.x, -r20.xyzw
            mov r19.y, r5.w
            mul r19.y, r19.y, c73.x
            frc r20.xyzw, r19.y
            add r19.y, r19.y, -r20.x
            mul r19.y, r19.y, c72.x
            add r19.x, r19.x, -r19.y
            mov r19.y, r5.w
            mul r19.y, r19.y, c73.x
            frc r20.xyzw, r19.y
            add r19.y, r19.y, -r20.x
            mov r14.x, r15.w
            mov r14.y, r19.x
            mov r14.z, r19.y
            mov r19.xyz, r14.xyzx
            add r19.xyz, r5.xyzx, r19.xyzx
            rcp r13.x, c20.x
            rcp r13.y, c20.x
            rcp r13.z, c20.x
            mul r20.xyz, r19.xyzx, r13.xyzx
            frc r21.xyzw, r20.xyzx
            add r20.xyz, r20.xyzx, -r21.xyzw
            mul r20.xyz, c20.x, r20.xyzx
            add r20.xyz, r19.xyzx, -r20.xyzx
            mul r15.w, r20.z, c20.x
            add r15.w, r20.x, r15.w
            mov r12.x, r15.w
            mov r12.y, r20.y
            mov r21.xy, r12.xyxx
            add r21.xy, r21.xyxx, c69.zwzz
            mul r15.w, c20.x, c20.x
            mov r11.y, r15.w
            mov r11.z, c20.x
            mov r21.zw, r11.xxyz
            rcp r12.z, r21.z
            rcp r12.w, r21.w
            mul r21.xy, r21.xyxx, r12.zwzz
            mov r17.xyzw, c68.xyzw
            mov r17.xy, r21.xyxx
            mov r22.xyzw, r17.xyzw
            texldl r22.xyzw, r22.xyzw, s10
            add r23.xyz, r22.xyzx, -r19.xyzx
            abs r23.xyz, r23.xyzx
            add r23.xyz, -r23.xyzx, -r23.xyzx
            cmp r23.xyz, r23.xyzx, c68.xyzx, c75.yzwy
            add r23.xyz, -r23.xyzx, c75.yzwy
            min r15.w, r23.x, r23.y
            min r15.w, r15.w, r23.z
            cmp r19.w, r22.w, c68.x, c70.y
            add r19.w, -r19.w, c70.y
            min r15.w, r15.w, r19.w
            if_ne r15.w, -r15.w
                add r23.xyz, -r2.xyzx, c75.yzwy
                mov r24.xyz, r14.xyzx
                add r25.xyz, r2.xyzx, -r23.xyzx
                mul r24.xyz, r24.xyzx, r25.xyzx
                add r23.xyz, r23.xyzx, r24.xyzx
                mul r15.w, r23.x, r23.y
                mul r15.w, r15.w, r23.z
                add r19.w, c24.w, -r22.w
                mul r19.w, r19.w, c76.x
                mov_sat r19.w, r19.w
                mul r15.w, r15.w, r19.w
                mov r19.w, r2.w
                add r19.w, r19.w, r15.w
                mov r2.w, r19.w
                mul r19.xyz, r19.xyzx, c19.w
                add r19.xyz, r1.xyzx, -r19.xyzx
                dp3 r19.w, r19.xyzx, r19.xyzx
                rsq r11.x, r19.w
                rcp r7.w, r11.x
                abs r22.xyz, r19.xyzx
                add r21.zw, r22.x, -r22.xxyz
                cmp r21.zw, r21.xxzw, c68.xxxy, c75.xxyz
                add r21.zw, -r21.xxzw, c75.xxyz
                min r19.w, r21.z, r21.w
                if_ne r19.w, -r19.w
                    cmp r19.w, r19.x, c68.x, c70.y
                    add r19.w, -r19.w, c70.y
                    cmp r19.w, -r19.w, c70.y, c68.x
                    mov r11.w, r19.w
                else
                    add r19.w, r22.y, -r22.z
                    cmp r19.w, r19.w, c68.x, c70.y
                    add r19.w, -r19.w, c70.y
                    cmp r20.w, r19.y, c68.x, c70.y
                    add r20.w, -r20.w, c70.y
                    cmp r20.w, -r20.w, c76.y, c72.x
                    mov r21.z, r11.w
                    cmp r21.z, -r19.w, r21.z, r20.w
                    mov r11.w, r21.z
                    cmp r19.x, r19.z, c68.x, c70.y
                    add r19.x, -r19.x, c70.y
                    cmp r19.x, -r19.x, c76.w, c76.z
                    cmp r19.x, -r19.w, r19.x, r20.w
                    mov r11.w, r19.x
                endif
                mov r19.x, r11.w
                mul r19.x, r19.x, c20.x
                add r19.x, r20.y, r19.x
                add r19.x, r19.x, c69.z
                mul r19.y, c20.x, c77.x
                rcp r14.w, r19.y
                mul r19.x, r19.x, r14.w
                mov r10.x, r21.x
                mov r10.y, r19.x
                mov r10.z, c68.x
                mov r10.w, c68.x
                mov r19.xyzw, r10.xyzw
                mov r8.xyzw, c68.xyzw
                mov r8.xy, r19.xyxx
                mov r19.xyzw, r8.xyzw
                texldl r19.xyzw, r19.xyzw, s7
                mul r20.x, r19.x, r19.x
                add r20.x, r19.y, -r20.x
                max r20.x, r20.x, c77.y
                add r20.y, r7.w, -r19.x
                max r20.y, r20.y, c68.x
                mul r20.y, r20.y, r20.y
                add r20.y, r20.x, r20.y
                rcp r13.w, r20.y
                mul r20.x, r20.x, r13.w
                mul r20.x, r20.x, r20.x
                mul r19.x, r20.x, r19.z
                mul r15.w, r15.w, r19.x
                mov r18.xyzw, c68.xyzw
                mov r18.xy, r21.xyxx
                mov r19.xyzw, r18.xyzw
                texldl r19.xyzw, r19.xyzw, s4
                dp4 r19.x, r19.xyzw, r6.xyzw
                mov r9.xyzw, c68.xyzw
                mov r9.xy, r21.xyxx
                mov r20.xyzw, r9.xyzw
                texldl r20.xyzw, r20.xyzw, s5
                dp4 r19.y, r20.xyzw, r6.xyzw
                mov r16.xyzw, c68.xyzw
                mov r16.xy, r21.xyxx
                mov r20.xyzw, r16.xyzw
                texldl r20.xyzw, r20.xyzw, s6
                dp4 r19.z, r20.xyzw, r6.xyzw
                mov r15.x, r19.x
                mov r15.y, r19.y
                mov r15.z, r19.z
                mov r19.xyz, r15.xyzx
                max r19.xyz, r19.xyzx, c68.xyzx
                mul r19.xyz, r19.xyzx, r15.w
                mov r20.xyz, r7.xyzx
                add r19.xyz, r20.xyzx, r19.xyzx
                mov r7.xyz, r19.xyzx
                mov r19.x, r1.w
                add r15.w, r19.x, r15.w
                mov r1.w, r15.w
            else
            endif
            mov r15.w, r5.w
            add r15.w, r15.w, c70.y
            mov r5.w, r15.w
        endrep
        mov r1.x, r1.w
        add r1.x, -r1.x, c70.w
        cmp r1.x, r1.x, c68.x, c70.y
        mov r2.xyz, r7.xyzx
        mov r1.y, r1.w
        rcp r1.y, r1.y
        mov r1.yzw, r1.y
        mul r1.yzw, r2.xxyz, r1.xyzw
        mov r2.x, r2.w
        mov_sat r2.x, r2.x
        mov r5.xyz, r1.yzwy
        mov r5.w, r2.x
        mov r2.xyzw, r5.xyzw
        cmp r1.xyzw, -r1.x, c68.xyzw, r2.xyzw
        mov r4.xyzw, r1.xyzw
    else
    endif
    mov r1.xyzw, r4.xyzw
    mul r2.xyz, r1.xyzx, c78.xyzx
    add r2.xyz, r2.xyzx, -c18.xyzx
    mul r2.xyz, r2.xyzx, c20.y
    mul r1.xyz, r2.xyzx, r1.w
    min r2.xyz, r1.xyzx, c68.xyzx
    max r1.xyz, r1.xyzx, c68.xyzx
    mul r0.xyz, r1.xyzx, r0.w
    add r0.xyz, r2.xyzx, r0.xyzx
    add r1.xyz, c18.xyzx, r0.xyzx
    max r1.xyz, r1.xyzx, c68.xyzx
    mul r1.xyz, r1.xyzx, c18.w
    add r0.xyz, r0.xyzx, r1.xyzx
    mov r0.w, c68.x
    mov r3.xyzw, r0.xyzw
else
endif
mov oC0.xyzw, r3.xyzw
