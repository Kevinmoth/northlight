ps_3_0
def c49 = 0.00000000e+00, -3.20000000e+01, 1.00000000e+00, -1.00000000e+00
def c50 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c51 = -2.00000000e+00, -3.00000000e+00, -4.00000000e+00, -5.00000000e+00
def c52 = -6.00000000e+00, -7.00000000e+00, -8.00000000e+00, -9.00000000e+00
def c53 = -1.00000000e+01, -1.10000000e+01, -1.20000000e+01, -1.30000000e+01
def c54 = -1.40000000e+01, -1.50000000e+01, -1.60000000e+01, -1.70000000e+01
def c55 = -1.80000000e+01, -1.90000000e+01, -2.00000000e+01, -2.10000000e+01
def c56 = -2.20000000e+01, -2.30000000e+01, -2.40000000e+01, -2.50000000e+01
def c57 = -2.60000000e+01, -2.70000000e+01, -2.80000000e+01, -2.90000000e+01
def c58 = -3.00000000e+01, -3.10000000e+01, 5.00000000e-01, -9.99999940e-01
def c59 = 5.00000000e-01, 5.00000000e-01, -5.00000000e-01, -5.00000000e-01
def c60 = 3.00000003e-03, 3.12500000e-02, 2.00000000e+00, -1.00000005e-03
def c61 = 5.00000000e-01, 5.00000000e-01, 0.00000000e+00, 0.00000000e+00
defi i0 = 255, 0, 0, 0
dcl_2d s0
dcl_2d s3
dcl_2d s4
dcl_2d s2
mov r0.x, c49.x
mov r0.y, c49.x
rep i0.xyzw
    mov r12.y, r0.y
    add r12.y, r12.y, c49.y
    cmp r12.y, r12.y, c49.x, c49.z
    add r12.y, -r12.y, c49.z
    if_ne r12.y, -r12.y
        break
    else
    endif
    mov r12.y, r0.y
    abs r12.z, r12.y
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, c50.xyzw, c13.xyzw
    add r12.z, r12.y, c49.w
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c14.xyzw
    add r12.z, r12.y, c51.x
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c15.xyzw
    add r12.z, r12.y, c51.y
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c16.xyzw
    add r12.z, r12.y, c51.z
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c17.xyzw
    add r12.z, r12.y, c51.w
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c18.xyzw
    add r12.z, r12.y, c52.x
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c19.xyzw
    add r12.z, r12.y, c52.y
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c20.xyzw
    add r12.z, r12.y, c52.z
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c21.xyzw
    add r12.z, r12.y, c52.w
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c22.xyzw
    add r12.z, r12.y, c53.x
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c23.xyzw
    add r12.z, r12.y, c53.y
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c24.xyzw
    add r12.z, r12.y, c53.z
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c25.xyzw
    add r12.z, r12.y, c53.w
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c26.xyzw
    add r12.z, r12.y, c54.x
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c27.xyzw
    add r12.z, r12.y, c54.y
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c28.xyzw
    add r12.z, r12.y, c54.z
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c29.xyzw
    add r12.z, r12.y, c54.w
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c30.xyzw
    add r12.z, r12.y, c55.x
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c31.xyzw
    add r12.z, r12.y, c55.y
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c32.xyzw
    add r12.z, r12.y, c55.z
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c33.xyzw
    add r12.z, r12.y, c55.w
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c34.xyzw
    add r12.z, r12.y, c56.x
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c35.xyzw
    add r12.z, r12.y, c56.y
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c36.xyzw
    add r12.z, r12.y, c56.z
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c37.xyzw
    add r12.z, r12.y, c56.w
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c38.xyzw
    add r12.z, r12.y, c57.x
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c39.xyzw
    add r12.z, r12.y, c57.y
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c40.xyzw
    add r12.z, r12.y, c57.z
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c41.xyzw
    add r12.z, r12.y, c57.w
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c42.xyzw
    add r12.z, r12.y, c58.x
    abs r12.z, r12.z
    add r12.z, -r12.z, -r12.z
    cmp r12.z, r12.z, c49.x, c49.z
    add r12.z, -r12.z, c49.z
    cmp r13.xyzw, -r12.z, r13.xyzw, c43.xyzw
    add r12.y, r12.y, c58.y
    abs r12.y, r12.y
    add r12.y, -r12.y, -r12.y
    cmp r12.y, r12.y, c49.x, c49.z
    add r12.y, -r12.y, c49.z
    cmp r13.xyzw, -r12.y, r13.xyzw, c44.xyzw
    add r12.y, -c46.x, c58.z
    cmp r12.y, r12.y, c49.x, c49.z
    mov r5.xyzw, c50.xyzw
    mov r5.xy, r13.zwzz
    mov r14.xyzw, r5.xyzw
    texldl r14.xyzw, r14.xyzw, s4
    mov_sat r12.z, r14.x
    add r12.z, -r12.z, c49.z
    cmp r12.y, -r12.y, c49.z, r12.z
    rcp r0.z, c0.x
    rcp r0.w, c0.y
    add r12.zw, r0.xxzw, c59.xxxy
    frc r14.xyzw, r12.zwzz
    add r12.zw, r12.xxzw, -r14.xxxy
    mul r12.zw, r13.xxxy, r12.xxzw
    add r12.zw, r12.xxzw, c59.xxzw
    frc r14.xyzw, r12.zwzz
    add r14.xy, r12.zwzz, -r14.xyzw
    frc r12.zw, r12.xxzw
    add r14.xy, r14.xyxx, c59.xyxx
    mul r14.xy, r14.xyxx, c0.xyxx
    cmp r13.x, r13.x, c49.x, c49.z
    add r13.x, -r13.x, c49.z
    cmp r13.x, -r13.x, c49.x, c49.z
    add r13.y, r14.x, c0.x
    add r13.z, r14.y, c0.y
    mov r2.xy, r14.xyxx
    mov r2.z, c49.x
    mov r2.w, c49.x
    mov r7.xyzw, c50.xyzw
    mov r7.xy, r14.xyxx
    mov r15.xyzw, r7.xyzw
    texldl r15.xyzw, r15.xyzw, s0
    add r13.w, r15.x, -c0.z
    mul r13.w, r13.w, c0.w
    mov_sat r13.w, r13.w
    add r13.x, r13.x, c59.z
    cmp r13.x, r13.x, c49.x, c49.z
    add r13.x, -r13.x, c49.z
    add r13.w, r13.w, c58.w
    cmp r13.w, r13.w, c49.x, c49.z
    add r13.w, -r13.w, c49.z
    min r13.w, r13.x, r13.w
    cmp r13.w, -r13.w, c49.x, c49.z
    mov r12.x, r13.w
    add r14.z, -c11.y, c58.z
    cmp r14.z, r14.z, c49.x, c49.z
    add r14.w, c11.x, -c11.z
    add r15.x, r15.x, -r14.w
    cmp r15.x, r15.x, c49.x, c49.z
    min r15.x, r14.z, r15.x
    cmp r13.w, -r15.x, r13.w, c49.x
    mov r12.x, r13.w
    add r15.x, -c1.w, c58.z
    cmp r15.x, r15.x, c49.x, c49.z
    if_ne r15.x, -r15.x
        mov r4.xy, r14.xyxx
        mov r4.z, c49.x
        mov r4.w, c49.x
        mov r8.xyzw, c50.xyzw
        mov r8.xy, r14.xyxx
        mov r16.xyzw, r8.xyzw
        texldl r16.xyzw, r16.xyzw, s2
        add r15.y, c10.y, -r16.x
        cmp r15.y, r15.y, c49.x, c49.z
        add r15.z, c10.z, -r16.x
        cmp r15.z, r15.z, c49.x, c49.z
        add r15.z, -r15.z, c49.z
        min r15.y, r15.y, r15.z
        add r15.z, -r16.y, c60.x
        cmp r15.z, r15.z, c49.x, c49.z
        min r15.y, r15.y, r15.z
        cmp r15.y, -r15.y, c49.x, c49.z
        add r15.y, -r15.y, c49.z
        mul r13.w, r13.w, r15.y
        mov r12.x, r13.w
    else
    endif
    mov r13.w, r12.x
    mov r2.x, r13.y
    mov r2.y, r14.y
    mov r2.z, c49.x
    mov r2.w, c49.x
    mov r16.xyzw, r2.xyzw
    mov r10.xyzw, c50.xyzw
    mov r10.xy, r16.xyxx
    mov r16.xyzw, r10.xyzw
    texldl r16.xyzw, r16.xyzw, s0
    add r15.y, r16.x, -c0.z
    mul r15.y, r15.y, c0.w
    mov_sat r15.y, r15.y
    add r15.y, r15.y, c58.w
    cmp r15.y, r15.y, c49.x, c49.z
    add r15.y, -r15.y, c49.z
    min r15.y, r13.x, r15.y
    cmp r15.y, -r15.y, c49.x, c49.z
    mov r12.x, r15.y
    add r15.z, r16.x, -r14.w
    cmp r15.z, r15.z, c49.x, c49.z
    min r15.z, r14.z, r15.z
    cmp r15.y, -r15.z, r15.y, c49.x
    mov r12.x, r15.y
    if_ne r15.x, -r15.x
        mov r4.x, r13.y
        mov r4.y, r14.y
        mov r4.z, c49.x
        mov r4.w, c49.x
        mov r16.xyzw, r4.xyzw
        mov r11.xyzw, c50.xyzw
        mov r11.xy, r16.xyxx
        mov r16.xyzw, r11.xyzw
        texldl r16.xyzw, r16.xyzw, s2
        add r15.z, c10.y, -r16.x
        cmp r15.z, r15.z, c49.x, c49.z
        add r15.w, c10.z, -r16.x
        cmp r15.w, r15.w, c49.x, c49.z
        add r15.w, -r15.w, c49.z
        min r15.z, r15.z, r15.w
        add r15.w, -r16.y, c60.x
        cmp r15.w, r15.w, c49.x, c49.z
        min r15.z, r15.z, r15.w
        cmp r15.z, -r15.z, c49.x, c49.z
        add r15.z, -r15.z, c49.z
        mul r15.y, r15.y, r15.z
        mov r12.x, r15.y
    else
    endif
    mov r15.y, r12.x
    add r15.y, r15.y, -r13.w
    mul r15.y, r12.z, r15.y
    add r13.w, r13.w, r15.y
    mov r2.x, r14.x
    mov r2.y, r13.z
    mov r2.z, c49.x
    mov r2.w, c49.x
    mov r16.xyzw, r2.xyzw
    mov r9.xyzw, c50.xyzw
    mov r9.xy, r16.xyxx
    mov r16.xyzw, r9.xyzw
    texldl r16.xyzw, r16.xyzw, s0
    add r15.y, r16.x, -c0.z
    mul r15.y, r15.y, c0.w
    mov_sat r15.y, r15.y
    add r15.y, r15.y, c58.w
    cmp r15.y, r15.y, c49.x, c49.z
    add r15.y, -r15.y, c49.z
    min r15.y, r13.x, r15.y
    cmp r15.y, -r15.y, c49.x, c49.z
    mov r12.x, r15.y
    add r15.z, r16.x, -r14.w
    cmp r15.z, r15.z, c49.x, c49.z
    min r15.z, r14.z, r15.z
    cmp r15.y, -r15.z, r15.y, c49.x
    mov r12.x, r15.y
    if_ne r15.x, -r15.x
        mov r4.x, r14.x
        mov r4.y, r13.z
        mov r4.z, c49.x
        mov r4.w, c49.x
        mov r16.xyzw, r4.xyzw
        mov r6.xyzw, c50.xyzw
        mov r6.xy, r16.xyxx
        mov r16.xyzw, r6.xyzw
        texldl r16.xyzw, r16.xyzw, s2
        add r14.x, c10.y, -r16.x
        cmp r14.x, r14.x, c49.x, c49.z
        add r14.y, c10.z, -r16.x
        cmp r14.y, r14.y, c49.x, c49.z
        add r14.y, -r14.y, c49.z
        min r14.x, r14.x, r14.y
        add r14.y, -r16.y, c60.x
        cmp r14.y, r14.y, c49.x, c49.z
        min r14.x, r14.x, r14.y
        cmp r14.x, -r14.x, c49.x, c49.z
        add r14.x, -r14.x, c49.z
        mul r14.x, r15.y, r14.x
        mov r12.x, r14.x
    else
    endif
    mov r14.x, r12.x
    mov r2.x, r13.y
    mov r2.y, r13.z
    mov r2.z, c49.x
    mov r2.w, c49.x
    mov r16.xyzw, r2.xyzw
    mov r1.xyzw, c50.xyzw
    mov r1.xy, r16.xyxx
    mov r16.xyzw, r1.xyzw
    texldl r16.xyzw, r16.xyzw, s0
    add r14.y, r16.x, -c0.z
    mul r14.y, r14.y, c0.w
    mov_sat r14.y, r14.y
    add r14.y, r14.y, c58.w
    cmp r14.y, r14.y, c49.x, c49.z
    add r14.y, -r14.y, c49.z
    min r13.x, r13.x, r14.y
    cmp r13.x, -r13.x, c49.x, c49.z
    mov r12.x, r13.x
    add r14.y, r16.x, -r14.w
    cmp r14.y, r14.y, c49.x, c49.z
    min r14.y, r14.z, r14.y
    cmp r13.x, -r14.y, r13.x, c49.x
    mov r12.x, r13.x
    if_ne r15.x, -r15.x
        mov r4.x, r13.y
        mov r4.y, r13.z
        mov r4.z, c49.x
        mov r4.w, c49.x
        mov r15.xyzw, r4.xyzw
        mov r3.xyzw, c50.xyzw
        mov r3.xy, r15.xyxx
        mov r15.xyzw, r3.xyzw
        texldl r15.xyzw, r15.xyzw, s2
        add r13.y, c10.y, -r15.x
        cmp r13.y, r13.y, c49.x, c49.z
        add r13.z, c10.z, -r15.x
        cmp r13.z, r13.z, c49.x, c49.z
        add r13.z, -r13.z, c49.z
        min r13.y, r13.y, r13.z
        add r13.z, -r15.y, c60.x
        cmp r13.z, r13.z, c49.x, c49.z
        min r13.y, r13.y, r13.z
        cmp r13.y, -r13.y, c49.x, c49.z
        add r13.y, -r13.y, c49.z
        mul r13.x, r13.x, r13.y
        mov r12.x, r13.x
    else
    endif
    mov r13.x, r12.x
    add r13.x, r13.x, -r14.x
    mul r13.x, r12.z, r13.x
    add r13.x, r14.x, r13.x
    add r13.x, r13.x, -r13.w
    mul r12.z, r12.w, r13.x
    add r12.z, r13.w, r12.z
    mul r12.y, r12.z, r12.y
    mov r12.z, r0.x
    add r12.y, r12.z, r12.y
    mov r0.x, r12.y
    mov r12.y, r0.y
    add r12.y, r12.y, c49.z
    mov r0.y, r12.y
endrep
mul r0.x, r0.x, c60.y
add r0.y, -r0.x, c60.z
mul r0.x, r0.x, r0.y
mov r0.y, r0.x
add r0.z, -c45.x, c58.z
cmp r0.z, r0.z, c49.x, c49.z
if_ne r0.z, -r0.z
    texldl r1.xyzw, c61.xyzw, s3
    mov_sat r0.z, r1.x
    add r0.w, r0.x, -r0.z
    cmp r1.x, r0.w, c49.x, c49.z
    cmp r1.x, -r1.x, c45.z, c45.y
    mul r0.w, r1.x, r0.w
    add r0.z, r0.z, r0.w
    mov r0.y, r0.z
else
endif
cmp r0.x, -r0.x, c49.x, c49.z
add r0.x, -r0.x, c49.z
mov r0.z, r0.y
add r0.z, r0.z, c60.w
cmp r0.z, r0.z, c49.x, c49.z
min r0.x, r0.x, r0.z
cmp r0.x, -r0.x, r0.y, c49.x
mov_sat r0.x, r0.x
mov oC0.xyzw, r0.x
