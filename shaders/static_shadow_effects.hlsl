// Compact, dedicated caster path. Same affine row-vector projection as ShadowVS.
row_major float4x4 LightMatrix:register(c0);
float4 WorldX:register(c4);
float4 WorldY:register(c5);
float4 WorldZ:register(c6);
struct CasterOutput { float4 position:POSITION0; float2 uv:TEXCOORD0; float depth:TEXCOORD1; };
CasterOutput CasterProject(float3 p,float2 uv,float4 x,float4 y,float4 z) {
    float4 q=float4(p,1);float3 world=float3(dot(q,x),dot(q,y),dot(q,z));
    CasterOutput o;o.position=mul(float4(world,1),LightMatrix);o.uv=uv;o.depth=o.position.z;
    // Preserve raw affine depth for exact fragment depth and far rejection.
    // Clamp raster depth on BOTH ends: clamping just the near vertices would
    // otherwise change where hardware far-clips a triangle spanning both planes.
    o.position.z=saturate(o.position.z);return o;
}
CasterOutput StaticCasterVS(float3 p:POSITION0,float2 uv:TEXCOORD0) {return CasterProject(p,uv,WorldX,WorldY,WorldZ);}
CasterOutput StaticCasterInstancedVS(float3 p:POSITION0,float2 uv:TEXCOORD0,
    float4 x:TEXCOORD1,float4 y:TEXCOORD2,float4 z:TEXCOORD3) {return CasterProject(p,uv,x,y,z);}
sampler2D Alpha:register(s0);
float4 Material:register(c0);
struct CasterDepth {float4 color:COLOR0;float depth:DEPTH;};
CasterDepth StaticCasterPS(float2 uv:TEXCOORD0,float depth:TEXCOORD1) {
    clip(tex2D(Alpha,uv).a-Material.w);clip(1-depth);
    CasterDepth o;o.depth=max(depth,0);o.color=o.depth.xxxx;return o;
}

// Bounds proved strictly inside BOTH depth planes: raster and raw depth are
// identical, so omit DEPTH output and retain early-depth-test eligibility.
float4 StaticCasterFastPS(float2 uv:TEXCOORD0,float depth:TEXCOORD1):COLOR0 {
    clip(tex2D(Alpha,uv).a-Material.w);return depth.xxxx;
}
float4 StaticCasterOpaqueFastPS(float depth:TEXCOORD1):COLOR0 {return depth.xxxx;}
// Opaque coverage skips the texture only. Crossing/uncertain geometry retains
// the exact 0.3.95 far rejection and explicit fragment depth behavior.
CasterDepth StaticCasterOpaquePS(float depth:TEXCOORD1) {
    clip(1-depth);CasterDepth o;o.depth=max(depth,0);o.color=o.depth.xxxx;return o;
}
