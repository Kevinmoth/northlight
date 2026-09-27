ps_3_0
dcl_texcoord1 v0
def c7 = 1.00000000e+00, 0.00000000e+00, 0.00000000e+00, 0.00000000e+00
add r0.x, -v0.x, c7.x
texkill r0.x
max r0.x, v0.x, c7.y
mov oC0.xyzw, r0.x
mov oDepth, r0.x
