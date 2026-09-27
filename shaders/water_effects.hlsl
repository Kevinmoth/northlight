// Exact liquid geometry mask. Visible-scene SSR; sky fallback has no off-screen geometry.
sampler2D Scene:register(s0);
sampler2D Depth:register(s1);
sampler2D Mask:register(s2);
sampler2D Reflection:register(s3);
sampler2D ShadowNear:register(s4);
sampler2D ShadowFar:register(s5);
sampler2D MoonShadowNear:register(s6);
sampler2D MoonShadowFar:register(s7);
float4 ImageClip:register(c0); // invW,invH,near,far
float4 Projection:register(c1); // signedX,signedY,Zsign,minZ
float4 WaterInfo:register(c2); // invDepthRange,seconds,reflectionStrength,foamStrength
row_major float4x4 InverseView:register(c3);
float4 SunDirection:register(c7);
float4 SunColor:register(c8);
float4 MoonDirection:register(c9);
float4 MoonColor:register(c10);
float4 SkyColor:register(c11);
row_major float4x4 NearMatrix:register(c12);
row_major float4x4 FarMatrix:register(c16);
row_major float4x4 MoonNearMatrix:register(c20);
row_major float4x4 MoonFarMatrix:register(c24);
float4 ShadowInfo:register(c28); // sun valid,moon valid,texel,unused
float2 centerUV(float2 uv){float2 dimensions=floor(1/ImageClip.xy+.5);return (clamp(floor(uv*dimensions),0,dimensions-1)+.5)*ImageClip.xy;}
float sceneZ(float2 uv){float d=saturate((tex2Dlod(Depth,float4(uv,0,0)).r-Projection.w)*WaterInfo.x);return ImageClip.z*ImageClip.w/max(ImageClip.w-d*(ImageClip.w-ImageClip.z),.00001);}
float3 viewPosition(float2 uv,float z){float2 xy=((uv-.5*ImageClip.xy)*float2(2,-2)+float2(-1,1))/Projection.xy;return float3(xy,Projection.z)*z;}
float3 toWorld(float3 v){return v.x*InverseView[0].xyz+v.y*InverseView[1].xyz+v.z*InverseView[2].xyz+InverseView[3].xyz;}
float3 toViewVector(float3 w){return float3(dot(w,InverseView[0].xyz),dot(w,InverseView[1].xyz),dot(w,InverseView[2].xyz));}
float2 project(float3 p){return (p.xy*Projection.xy/max(p.z*Projection.z,.001))*float2(.5,-.5)+.5+.5*ImageClip.xy;}
float validWater(float4 m,float opaqueZ){return (m.r>ImageClip.z && m.r<=opaqueZ+max(.012,m.r*.0007) && m.g>.003)?1:0;}
float3 waveNormal(float3 p,float footprintSquared){
    // Derivatives of three world-anchored travelling height waves, not UV sliding.
    float t=WaterInfo.y;
    float a=dot(p.xy,float2(.72,.31))-t*1.1;
    float b=dot(p.xy,float2(-.41,.89))+t*.73;
    float c=dot(p.xy,float2(1.73,-1.21))-t*1.63;
    float2 slope=float2(.72,.31)*cos(a)*.055+float2(-.41,.89)*cos(b)*.035+float2(1.73,-1.21)*cos(c)*.012;
    // Fade wavelengths before they cross the screen-space Nyquist limit.
    // Max wave vector length=2.112; pi/2.112=1.488 world units/pixel.
    slope*=saturate(1-footprintSquared*.452);
    return normalize(float3(-slope,1));
}
float3 affine(float3 p,row_major float4x4 m){return p.x*m[0].xyz+p.y*m[1].xyz+p.z*m[2].xyz+m[3].xyz;}
float shadow(float3 p,bool moon){
    if((moon?ShadowInfo.y:ShadowInfo.x)<.5)return 1;
    float3 n=moon?affine(p,MoonNearMatrix):affine(p,NearMatrix);float3 f=moon?affine(p,MoonFarMatrix):affine(p,FarMatrix);
    bool close=max(abs(n.x),abs(n.y))<.95;float3 l=close?n:f;
    float2 uv=float2(l.x*.5+.5,.5-l.y*.5)+.5*ShadowInfo.z;
    if(any(uv<.001)||any(uv>.999)||l.z<0||l.z>1)return 1;
    float visibility=0;
    [loop]for(int i=0;i<4;++i){float2 o=float2(frac(i*.5)*2-.5,floor(i*.5)-.5)*ShadowInfo.z;float d=moon?(close?tex2Dlod(MoonShadowNear,float4(uv+o,0,0)).r:tex2Dlod(MoonShadowFar,float4(uv+o,0,0)).r):(close?tex2Dlod(ShadowNear,float4(uv+o,0,0)).r:tex2Dlod(ShadowFar,float4(uv+o,0,0)).r);visibility+=(d>=l.z-.00012)? .25:0;}
    return visibility;
}
float4 WaterReflection(float2 inputUV:TEXCOORD0):COLOR0{
    float2 uv=centerUV(inputUV);float4 mask=tex2Dlod(Mask,float4(uv,0,0));float floorZ=sceneZ(uv);
    float3 v=viewPosition(uv,max(mask.r,ImageClip.z));float3 p=toWorld(v);
    // Derivatives must execute before the divergent water-mask branch.
    float3 dx=ddx(p),dy=ddy(p);float footprintSquared=max(dot(dx,dx),dot(dy,dy));
    if(validWater(mask,floorZ)<.5)return 0;
    float3 normal=waveNormal(p,footprintSquared);
    float3 incident=normalize(p-InverseView[3].xyz);if(dot(normal,-incident)<0)normal=-normal;
    float3 reflected=reflect(incident,normal);float3 ray=toViewVector(reflected);
    float3 sky=max(SkyColor.rgb,0)*( .45+.55*saturate(reflected.z));
    float3 result=sky;float confidence=0;float previous=0;float lastDelta=-1;
    [loop]for(int i=0;i<24;++i){
        if(reflected.z<=.002)break;
        float distance=.18+float(i)*.28+float(i*i)*.055;
        float3 marchPosition=v+ray*distance;float z=marchPosition.z*Projection.z;
        float2 sampleUV=project(marchPosition);
        if(z<=ImageClip.z||any(sampleUV<0)||any(sampleUV>1))break;
        float opaque=sceneZ(sampleUV);float delta=z-opaque;
        if(delta>=0 && lastDelta<0){
            // Reject gaps crossing foreground discontinuities. Refine the first depth crossing.
            float low=previous,high=distance;
            [unroll]for(int j=0;j<3;++j){float mid=(low+high)*.5;float3 q=v+ray*mid;float2 qUV=project(q);if(q.z*Projection.z>sceneZ(qUV))high=mid;else low=mid;}
            float3 hit=v+ray*high;float2 hitUV=centerUV(project(hit));float hitZ=sceneZ(hitUV);
            float4 otherWater=tex2Dlod(Mask,float4(hitUV,0,0));
            float thickness=max(.12,high*.035);float error=abs(hit.z*Projection.z-hitZ);
            float3 receiver=toWorld(viewPosition(hitUV,hitZ));
            // Reflection rays above a horizontal liquid cannot hit its bed.
            // Screen-depth discontinuities can otherwise invent those hits.
            if(error<thickness&&receiver.z>=p.z-.04&&hitZ<ImageClip.w*.995&&validWater(otherWater,hitZ)<.5){
                confidence=saturate(min(min(hitUV.x,hitUV.y),min(1-hitUV.x,1-hitUV.y))*14)*saturate(distance/1.5)*(1-smoothstep(thickness*.5,thickness,error));
                result=lerp(sky,tex2Dlod(Scene,float4(hitUV,0,0)).rgb,confidence);
            }
            break;
        }
        previous=distance;lastDelta=delta;
    }
    return float4(result,confidence);
}
float4 WaterComposite(float2 inputUV:TEXCOORD0):COLOR0{
    float2 uv=centerUV(inputUV);float4 original=tex2Dlod(Scene,float4(uv,0,0));float4 mask=tex2Dlod(Mask,float4(uv,0,0));float floorZ=sceneZ(uv);
    float3 p=toWorld(viewPosition(uv,max(mask.r,ImageClip.z)));
    float3 dx=ddx(p),dy=ddy(p);float footprintSquared=max(dot(dx,dx),dot(dy,dy));
    if(validWater(mask,floorZ)<.5)return original;
    // Reflection buffer is full resolution: exact mask pixels cannot bleed onto land.
    float4 reflection=tex2Dlod(Reflection,float4(uv,0,0));
    float coverage=saturate(mask.g*4);
    float3 normal=waveNormal(p,footprintSquared);float3 incident=normalize(p-InverseView[3].xyz);if(dot(normal,-incident)<0)normal=-normal;
    float3 reflected=reflect(incident,normal);float fresnel=.02037+.97963*pow(1-saturate(dot(-incident,normal)),5);
    float3 celestial=SunColor.rgb*pow(saturate(dot(reflected,SunDirection.xyz)),384)*6*shadow(p,false);
    celestial+=MoonColor.rgb*pow(saturate(dot(reflected,MoonDirection.xyz)),512)*4*shadow(p,true);
    float3 color=lerp(original.rgb,reflection.rgb+celestial*(1-reflection.a),saturate(fresnel*WaterInfo.z)*coverage);
    // The opaque receiver must actually be below the liquid plane. Vertical gap
    // uses reconstructed world coordinates, not view distance (wrong at grazing views).
    float3 floorPoint=toWorld(viewPosition(uv,floorZ));float gap=p.z-floorPoint.z;
    float depthQuantization=mask.r*mask.r*(4.0/16777215.0)/ImageClip.z;
    float shore=(floorZ-mask.r>depthQuantization&&gap>.015&&gap<.8)?(1-smoothstep(.08,.8,gap)):0;
    float pattern=.5+.5*sin(p.x*5.1+p.y*3.7+WaterInfo.y*1.5);
    float foam=shore*smoothstep(.35,.8,pattern)*WaterInfo.w*coverage*saturate(1-footprintSquared*4.03);
    color=lerp(color,SkyColor.rgb,saturate(foam));
    return float4(color,original.a);
}
