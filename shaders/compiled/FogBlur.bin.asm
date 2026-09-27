ps_3_0
dcl_texcoord0 v0
def c68 = 5.00000000e-01, 5.00000000e-01, -1.00000000e+00, -1.00000000e+00
def c69 = 0.00000000e+00, 0.00000000e+00, -5.00000000e-01, 1.00000000e+00
def c70 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c71 = 9.99999975e-06, 1.20000001e-02, 6.99999975e-04, 3.00000003e-03
def c72 = -2.00000000e+00, 2.00000000e+00, 6.00000000e+00, 4.00000000e+00
def c73 = 5.99999987e-02, 1.44269502e+00, 0.00000000e+00, 0.00000000e+00
defi i0 = 255, 0, 0, 0
dcl_2d s1
dcl_2d s9
dcl_2d s11
mul r0.xy, v0.xyxx, c33.zwzz
frc r1.xyzw, r0.xyxx
add r0.xy, r0.xyxx, -r1.xyzw
add r0.zw, r0.xxxy, c68.xxxy
rcp r1.x, c33.z
rcp r1.y, c33.w
mul r0.zw, r0.xxzw, r1.xxxy
mul r0.zw, r0.xxzw, c33.xxxy
frc r1.xyzw, r0.zwzz
add r0.zw, r0.xxzw, -r1.xxxy
add r1.xy, c33.xyxx, c68.zwzz
max r0.zw, r0.xxzw, c69.xxxy
min r0.zw, r0.xxzw, r1.xxxy
add r0.zw, r0.xxzw, c68.xxxy
mul r0.zw, r0.xxzw, c0.xxxy
mov r2.xyzw, c70.xyzw
mov r2.xy, r0.zwzz
texldl r2.xyzw, r2.xyzw, s1
add r1.z, r2.x, -c1.w
mul r1.z, r1.z, c2.x
mov_sat r1.z, r1.z
add r1.w, c30.x, c69.z
cmp r1.w, r1.w, c69.x, c69.w
mov r2.x, r2.y
cmp r2.x, -r1.w, r2.x, c69.x
mov r2.y, r2.x
add r2.x, -r1.w, c69.w
if_ne r2.x, -r2.x
    mov r3.xyzw, c70.xyzw
    mov r3.xy, r0.zwzz
    texldl r3.xyzw, r3.xyzw, s11
    add r0.z, c0.z, -r3.x
    cmp r0.z, r0.z, c69.x, c69.w
    mul r0.w, c0.z, c0.w
    add r2.z, c0.w, -c0.z
    mul r2.z, r1.z, r2.z
    add r2.z, c0.w, -r2.z
    max r2.z, r2.z, c71.x
    rcp r2.z, r2.z
    mul r0.w, r0.w, r2.z
    mul r2.z, r3.x, c71.z
    max r2.z, r2.z, c71.y
    add r0.w, r0.w, r2.z
    add r0.w, r0.w, -r3.x
    cmp r0.w, r0.w, c69.x, c69.w
    add r0.w, -r0.w, c69.w
    min r0.z, r0.z, r0.w
    add r0.w, -r3.y, c71.w
    cmp r0.w, r0.w, c69.x, c69.w
    min r0.z, r0.z, r0.w
    cmp r0.z, -r0.z, c69.x, r3.x
    mov r2.y, r0.z
else
endif
mov r0.z, r2.y
cmp r0.w, -r0.z, c69.x, c69.w
mul r2.z, c0.z, c0.w
add r2.w, c0.w, -c0.z
mul r1.z, r1.z, r2.w
add r1.z, c0.w, -r1.z
max r1.z, r1.z, c71.x
rcp r1.z, r1.z
mul r1.z, r2.z, r1.z
cmp r0.z, -r0.w, r1.z, r0.z
mov r3.x, c69.x
mov r3.y, c69.x
mov r3.z, c69.x
mov r3.w, c69.x
mov r0.w, c69.x
mov r1.z, c72.x
rep i0.xyzw
    mov r8.z, r1.z
    add r8.z, -r8.z, c72.y
    cmp r8.z, r8.z, c69.x, c69.w
    if_ne r8.z, -r8.z
        break
    else
    endif
    mov r8.z, r1.z
    mul r8.zw, c34.xxxy, r8.z
    add r8.zw, r0.xxxy, r8.xxzw
    add r9.xy, c33.zwzz, c68.zwzz
    max r8.zw, r8.xxzw, c69.xxxy
    min r8.zw, r8.xxzw, r9.xxxy
    add r8.zw, r8.xxzw, c68.xxxy
    rcp r8.x, c33.z
    rcp r8.y, c33.w
    mul r8.zw, r8.xxzw, r8.xxxy
    mul r9.xy, r8.zwzz, c33.xyxx
    frc r10.xyzw, r9.xyxx
    add r9.xy, r9.xyxx, -r10.xyzw
    max r9.xy, r9.xyxx, c69.xyxx
    min r9.xy, r9.xyxx, r1.xyxx
    add r9.xy, r9.xyxx, c68.xyxx
    mul r9.xy, r9.xyxx, c0.xyxx
    mov r5.xyzw, c70.xyzw
    mov r5.xy, r9.xyxx
    mov r10.xyzw, r5.xyzw
    texldl r10.xyzw, r10.xyzw, s1
    add r9.z, r10.x, -c1.w
    mul r9.z, r9.z, c2.x
    mov_sat r9.z, r9.z
    mov r9.w, r2.y
    cmp r9.w, -r1.w, r9.w, c69.x
    mov r2.y, r9.w
    if_ne r2.x, -r2.x
        mov r7.xyzw, c70.xyzw
        mov r7.xy, r9.xyxx
        mov r10.xyzw, r7.xyzw
        texldl r10.xyzw, r10.xyzw, s11
        add r9.x, c0.z, -r10.x
        cmp r9.x, r9.x, c69.x, c69.w
        mul r9.y, r9.z, r2.w
        add r9.y, c0.w, -r9.y
        max r9.y, r9.y, c71.x
        rcp r4.w, r9.y
        mul r9.y, r2.z, r4.w
        mul r9.w, r10.x, c71.z
        max r9.w, r9.w, c71.y
        add r9.y, r9.y, r9.w
        add r9.y, r9.y, -r10.x
        cmp r9.y, r9.y, c69.x, c69.w
        add r9.y, -r9.y, c69.w
        min r9.x, r9.x, r9.y
        add r9.y, -r10.y, c71.w
        cmp r9.y, r9.y, c69.x, c69.w
        min r9.x, r9.x, r9.y
        cmp r9.x, -r9.x, c69.x, r10.x
        mov r2.y, r9.x
    else
    endif
    mov r9.x, r2.y
    cmp r9.y, -r9.x, c69.x, c69.w
    mul r9.z, r9.z, r2.w
    add r9.z, c0.w, -r9.z
    max r9.z, r9.z, c71.x
    rcp r4.x, r9.z
    mul r9.z, r2.z, r4.x
    cmp r9.x, -r9.y, r9.z, r9.x
    mov r9.y, r1.z
    abs r9.y, r9.y
    add r9.y, -r9.y, -r9.y
    cmp r9.y, r9.y, c69.x, c69.w
    add r9.y, -r9.y, c69.w
    mov r9.z, r1.z
    max r9.z, r9.z, -r9.z
    add r9.z, r9.z, c68.z
    abs r9.z, r9.z
    add r9.z, -r9.z, -r9.z
    cmp r9.z, r9.z, c69.x, c69.w
    add r9.z, -r9.z, c69.w
    cmp r9.z, -r9.z, c69.w, c72.w
    cmp r9.y, -r9.y, r9.z, c72.z
    add r9.x, r9.x, -r0.z
    abs r9.x, r9.x
    mul r9.z, r0.z, c73.x
    max r9.z, r9.z, c68.x
    rcp r4.z, r9.z
    mul r9.x, -r9.x, r4.z
    mul r9.x, r9.x, c73.y
    exp r4.y, r9.x
    mul r9.x, r9.y, r4.y
    mov r6.xyzw, c70.xyzw
    mov r6.xy, r8.zwzz
    mov r10.xyzw, r6.xyzw
    texldl r10.xyzw, r10.xyzw, s9
    mul r10.xyzw, r10.xyzw, r9.x
    mov r11.xyzw, r3.xyzw
    add r10.xyzw, r11.xyzw, r10.xyzw
    mov r3.xyzw, r10.xyzw
    mov r8.z, r0.w
    add r8.z, r8.z, r9.x
    mov r0.w, r8.z
    mov r8.z, r1.z
    add r8.z, r8.z, c69.w
    mov r1.z, r8.z
endrep
mov r1.xyzw, r3.xyzw
mov r0.x, r0.w
max r0.x, r0.x, c71.x
rcp r2.x, r0.x
rcp r2.y, r0.x
rcp r2.z, r0.x
rcp r2.w, r0.x
mul oC0.xyzw, r1.xyzw, r2.xyzw
