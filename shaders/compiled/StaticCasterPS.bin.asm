ps_3_0
dcl_texcoord0 v0
dcl_texcoord1 v1
def c7 = 1.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
dcl_2d s0
texld r0.xyzw, v0.xyxx, s0
add r0.x, r0.w, -c0.w
texkill r0.x
add r0.x, -v1.x, c7.x
texkill r0.x
max r0.x, v1.x, c7.y
mov oC0.xyzw, r0.x
mov oDepth, r0.x
