ps_3_0
dcl_texcoord0 v0
dcl_texcoord1 v1
dcl_2d s0
texld r0.xyzw, v0.xyxx, s0
add r0.x, r0.w, -c0.w
texkill r0.x
mov oC0.xyzw, v1.x
