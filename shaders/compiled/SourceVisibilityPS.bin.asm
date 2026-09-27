ps_3_0
def c68 = 5.00000000e-01, 5.00000000e-01, 0.00000000e+00, 0.00000000e+00
def c69 = 1.00000000e+00, 5.00000007e-02, 5.00000000e-01, -5.00000000e-01
def c70 = 1.00000000e+00, 1.00000000e+00, -1.00000000e+00, -1.00000000e+00
def c71 = 0.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
def c72 = -9.99989986e-01, -3.33333343e-01, -1.00000000e+00, 6.25000000e-02
def c73 = 3.33333343e-01, -1.00000000e+00, -1.00000000e+00, -3.33333343e-01
def c74 = -3.33333343e-01, -3.33333343e-01, 3.33333343e-01, -3.33333343e-01
def c75 = 1.00000000e+00, -3.33333343e-01, -1.00000000e+00, 3.33333343e-01
def c76 = 3.33333343e-01, 3.33333343e-01, 1.00000000e+00, 3.33333343e-01
def c77 = -1.00000000e+00, 1.00000000e+00, -3.33333343e-01, 1.00000000e+00
def c78 = 2.50000000e+00, 0.00000000e+00, 2.47487378e+00, 2.47487378e+00
def c79 = -1.09278474e-07, 2.50000000e+00, -2.47487378e+00, 2.47487378e+00
def c80 = -2.50000000e+00, -2.18556949e-07, -2.47487330e+00, -2.47487402e+00
def c81 = 2.98122025e-08, -2.50000000e+00, 2.47487450e+00, -2.47487283e+00
def c82 = 6.00000024e-01, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
dcl_2d s1
dcl_2d s15
texldl r0.xyzw, c68.xyzw, s15
mov_sat r0.x, r0.x
dp3 r0.y, c16.xyzx, c3.xyzx
dp3 r0.z, c16.xyzx, c4.xyzx
dp3 r0.w, c16.xyzx, c5.xyzx
mov r1.x, r0.y
mov r1.y, r0.z
mov r1.z, r0.w
mul r0.y, r0.w, c1.z
mov r0.z, c69.x
add r0.w, -r0.y, c69.y
cmp r0.w, r0.w, c68.z, c69.x
if_ne r0.w, -r0.w
    mul r1.xy, r1.xyxx, c1.xyxx
    rcp r1.z, r0.y
    rcp r1.w, r0.y
    mul r0.yw, r1.xxxy, r1.xzxw
    mul r0.yw, r0.xyxw, c69.xzxw
    add r0.yw, r0.xyxw, c68.xxxy
    cmp r1.xy, -r0.ywyy, c68.zwzz, c70.xyxx
    min r1.x, r1.x, r1.y
    add r1.yz, r0.xywx, c70.xzwx
    cmp r1.yz, r1.xyzx, c68.xzwx, c70.xxyx
    min r1.y, r1.y, r1.z
    min r1.x, r1.x, r1.y
    if_ne r1.x, -r1.x
        mul r1.xy, c67.x, c1.xyxx
        mul r1.xy, r1.xyxx, c68.xyxx
        mul r1.zw, r1.xxxy, c70.xxzw
        add r1.zw, r0.xxyw, r1.xxzw
        mov_sat r1.zw, r1.xxzw
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r1.zwzz
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.z, r3.x, -c1.w
        mul r1.z, r1.z, c2.x
        add r1.z, r1.z, c72.x
        cmp r1.z, r1.z, c68.z, c69.x
        add r1.z, -r1.z, c69.x
        cmp r1.z, -r1.z, c68.z, c69.x
        mul r3.xy, r1.xyxx, c72.yzyy
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c73.xyxx
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c70.yzyy
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c73.zwzz
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c74.xyxx
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c74.zwzz
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c75.xyxx
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c75.zwzz
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c74.yzyy
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c76.xyxx
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c76.zwzz
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c77.xyxx
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c77.zwzz
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r3.xy, r1.xyxx, c76.yzyy
        add r3.xy, r0.ywyy, r3.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        mov r3.xyzw, r2.xyzw
        texldl r3.xyzw, r3.xyzw, s1
        add r1.w, r3.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        add r3.xy, r0.ywyy, r1.xyxx
        mov_sat r3.xy, r3.xyxx
        mov r2.xyzw, c71.xyzw
        mov r2.xy, r3.xyxx
        texldl r2.xyzw, r2.xyzw, s1
        add r1.w, r2.x, -c1.w
        mul r1.w, r1.w, c2.x
        add r1.w, r1.w, c72.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        cmp r1.w, -r1.w, c68.z, c69.x
        add r1.z, r1.z, r1.w
        mul r1.z, r1.z, c72.w
        mov r0.z, r1.z
        mul r2.xy, r1.xyxx, c78.xyxx
        add r2.xy, r0.ywyy, r2.xyxx
        add r1.w, r2.x, c69.w
        abs r1.w, r1.w
        add r1.w, -r1.w, c68.x
        cmp r1.w, r1.w, c68.z, c69.x
        add r1.w, -r1.w, c69.x
        add r2.z, r2.y, c69.w
        abs r2.z, r2.z
        add r2.z, -r2.z, c68.x
        cmp r2.z, r2.z, c68.z, c69.x
        add r2.z, -r2.z, c69.x
        mul r1.w, r1.w, r2.z
        mov r3.xyzw, c71.xyzw
        mov r3.xy, r2.xyxx
        mov r2.xyzw, r3.xyzw
        texldl r2.xyzw, r2.xyzw, s1
        add r2.x, r2.x, -c1.w
        mul r2.x, r2.x, c2.x
        add r2.x, r2.x, c72.x
        cmp r2.x, r2.x, c68.z, c69.x
        add r2.x, -r2.x, c69.x
        mul r2.x, r1.w, r2.x
        mul r2.yz, r1.xxyx, c78.xzwx
        add r2.yz, r0.xywx, r2.xyzx
        add r2.w, r2.y, c69.w
        abs r2.w, r2.w
        add r2.w, -r2.w, c68.x
        cmp r2.w, r2.w, c68.z, c69.x
        add r2.w, -r2.w, c69.x
        add r4.x, r2.z, c69.w
        abs r4.x, r4.x
        add r4.x, -r4.x, c68.x
        cmp r4.x, r4.x, c68.z, c69.x
        add r4.x, -r4.x, c69.x
        mul r2.w, r2.w, r4.x
        add r1.w, r1.w, r2.w
        mov r3.xyzw, c71.xyzw
        mov r3.xy, r2.yzyy
        mov r4.xyzw, r3.xyzw
        texldl r4.xyzw, r4.xyzw, s1
        add r2.y, r4.x, -c1.w
        mul r2.y, r2.y, c2.x
        add r2.y, r2.y, c72.x
        cmp r2.y, r2.y, c68.z, c69.x
        add r2.y, -r2.y, c69.x
        mul r2.y, r2.w, r2.y
        add r2.x, r2.x, r2.y
        mul r2.yz, r1.xxyx, c79.xxyx
        add r2.yz, r0.xywx, r2.xyzx
        add r2.w, r2.y, c69.w
        abs r2.w, r2.w
        add r2.w, -r2.w, c68.x
        cmp r2.w, r2.w, c68.z, c69.x
        add r2.w, -r2.w, c69.x
        add r4.x, r2.z, c69.w
        abs r4.x, r4.x
        add r4.x, -r4.x, c68.x
        cmp r4.x, r4.x, c68.z, c69.x
        add r4.x, -r4.x, c69.x
        mul r2.w, r2.w, r4.x
        add r1.w, r1.w, r2.w
        mov r3.xyzw, c71.xyzw
        mov r3.xy, r2.yzyy
        mov r4.xyzw, r3.xyzw
        texldl r4.xyzw, r4.xyzw, s1
        add r2.y, r4.x, -c1.w
        mul r2.y, r2.y, c2.x
        add r2.y, r2.y, c72.x
        cmp r2.y, r2.y, c68.z, c69.x
        add r2.y, -r2.y, c69.x
        mul r2.y, r2.w, r2.y
        add r2.x, r2.x, r2.y
        mul r2.yz, r1.xxyx, c79.xzwx
        add r2.yz, r0.xywx, r2.xyzx
        add r2.w, r2.y, c69.w
        abs r2.w, r2.w
        add r2.w, -r2.w, c68.x
        cmp r2.w, r2.w, c68.z, c69.x
        add r2.w, -r2.w, c69.x
        add r4.x, r2.z, c69.w
        abs r4.x, r4.x
        add r4.x, -r4.x, c68.x
        cmp r4.x, r4.x, c68.z, c69.x
        add r4.x, -r4.x, c69.x
        mul r2.w, r2.w, r4.x
        add r1.w, r1.w, r2.w
        mov r3.xyzw, c71.xyzw
        mov r3.xy, r2.yzyy
        mov r4.xyzw, r3.xyzw
        texldl r4.xyzw, r4.xyzw, s1
        add r2.y, r4.x, -c1.w
        mul r2.y, r2.y, c2.x
        add r2.y, r2.y, c72.x
        cmp r2.y, r2.y, c68.z, c69.x
        add r2.y, -r2.y, c69.x
        mul r2.y, r2.w, r2.y
        add r2.x, r2.x, r2.y
        mul r2.yz, r1.xxyx, c80.xxyx
        add r2.yz, r0.xywx, r2.xyzx
        add r2.w, r2.y, c69.w
        abs r2.w, r2.w
        add r2.w, -r2.w, c68.x
        cmp r2.w, r2.w, c68.z, c69.x
        add r2.w, -r2.w, c69.x
        add r4.x, r2.z, c69.w
        abs r4.x, r4.x
        add r4.x, -r4.x, c68.x
        cmp r4.x, r4.x, c68.z, c69.x
        add r4.x, -r4.x, c69.x
        mul r2.w, r2.w, r4.x
        add r1.w, r1.w, r2.w
        mov r3.xyzw, c71.xyzw
        mov r3.xy, r2.yzyy
        mov r4.xyzw, r3.xyzw
        texldl r4.xyzw, r4.xyzw, s1
        add r2.y, r4.x, -c1.w
        mul r2.y, r2.y, c2.x
        add r2.y, r2.y, c72.x
        cmp r2.y, r2.y, c68.z, c69.x
        add r2.y, -r2.y, c69.x
        mul r2.y, r2.w, r2.y
        add r2.x, r2.x, r2.y
        mul r2.yz, r1.xxyx, c80.xzwx
        add r2.yz, r0.xywx, r2.xyzx
        add r2.w, r2.y, c69.w
        abs r2.w, r2.w
        add r2.w, -r2.w, c68.x
        cmp r2.w, r2.w, c68.z, c69.x
        add r2.w, -r2.w, c69.x
        add r4.x, r2.z, c69.w
        abs r4.x, r4.x
        add r4.x, -r4.x, c68.x
        cmp r4.x, r4.x, c68.z, c69.x
        add r4.x, -r4.x, c69.x
        mul r2.w, r2.w, r4.x
        add r1.w, r1.w, r2.w
        mov r3.xyzw, c71.xyzw
        mov r3.xy, r2.yzyy
        mov r4.xyzw, r3.xyzw
        texldl r4.xyzw, r4.xyzw, s1
        add r2.y, r4.x, -c1.w
        mul r2.y, r2.y, c2.x
        add r2.y, r2.y, c72.x
        cmp r2.y, r2.y, c68.z, c69.x
        add r2.y, -r2.y, c69.x
        mul r2.y, r2.w, r2.y
        add r2.x, r2.x, r2.y
        mul r2.yz, r1.xxyx, c81.xxyx
        add r2.yz, r0.xywx, r2.xyzx
        add r2.w, r2.y, c69.w
        abs r2.w, r2.w
        add r2.w, -r2.w, c68.x
        cmp r2.w, r2.w, c68.z, c69.x
        add r2.w, -r2.w, c69.x
        add r4.x, r2.z, c69.w
        abs r4.x, r4.x
        add r4.x, -r4.x, c68.x
        cmp r4.x, r4.x, c68.z, c69.x
        add r4.x, -r4.x, c69.x
        mul r2.w, r2.w, r4.x
        add r1.w, r1.w, r2.w
        mov r3.xyzw, c71.xyzw
        mov r3.xy, r2.yzyy
        mov r4.xyzw, r3.xyzw
        texldl r4.xyzw, r4.xyzw, s1
        add r2.y, r4.x, -c1.w
        mul r2.y, r2.y, c2.x
        add r2.y, r2.y, c72.x
        cmp r2.y, r2.y, c68.z, c69.x
        add r2.y, -r2.y, c69.x
        mul r2.y, r2.w, r2.y
        add r2.x, r2.x, r2.y
        mul r1.xy, r1.xyxx, c81.zwzz
        add r0.yw, r0.xyxw, r1.xxxy
        add r1.x, r0.y, c69.w
        abs r1.x, r1.x
        add r1.x, -r1.x, c68.x
        cmp r1.x, r1.x, c68.z, c69.x
        add r1.x, -r1.x, c69.x
        add r1.y, r0.w, c69.w
        abs r1.y, r1.y
        add r1.y, -r1.y, c68.x
        cmp r1.y, r1.y, c68.z, c69.x
        add r1.y, -r1.y, c69.x
        mul r1.x, r1.x, r1.y
        add r1.y, r1.w, r1.x
        mov r3.xyzw, c71.xyzw
        mov r3.xy, r0.ywyy
        texldl r3.xyzw, r3.xyzw, s1
        add r0.y, r3.x, -c1.w
        mul r0.y, r0.y, c2.x
        add r0.y, r0.y, c72.x
        cmp r0.y, r0.y, c68.z, c69.x
        add r0.y, -r0.y, c69.x
        mul r0.y, r1.x, r0.y
        add r0.y, r2.x, r0.y
        mul r0.y, r0.y, c82.x
        max r0.w, r1.y, c69.x
        rcp r0.w, r0.w
        mul r0.y, r0.y, r0.w
        max r0.y, r1.z, r0.y
        mov r0.z, r0.y
    else
    endif
else
endif
mov r0.y, r0.z
add r0.x, r0.x, -r0.y
mul r0.x, c67.y, r0.x
add r0.x, r0.y, r0.x
mov oC0.xyzw, r0.x
