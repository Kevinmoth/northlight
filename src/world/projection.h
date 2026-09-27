#pragma once
#include <cmath>
struct Projection { float nearZ, farZ, scaleX, scaleY; };
inline bool decodeProjection(const float* p, Projection& out) {
    for (int i=0;i<16;++i) if (!std::isfinite(p[i])) return false;
    const int zeros[]={1,2,3,4,6,7,8,9,12,13,15};
    for (int i:zeros) if (std::fabs(p[i])>.002f) return false;
    const float A=p[10],B=p[14],C=p[11];
    if (std::fabs(C)<.99f||std::fabs(C)>1.01f||A/C<=1.f||B>=0.f||std::fabs(A-C)<1e-7f) return false;
    const float n=std::fabs(-B/A),f=std::fabs(B/(C-A));
    const float sx=std::fabs(p[0]/C),sy=std::fabs(p[5]/C);
    if (!(n>.001f&&n<10&&f>n*10&&f<100000&&sx>.1f&&sx<10&&sy>.1f&&sy<10)) return false;
    out={n,f,sx,sy};return true;
}
// WMO/model shaders store the projection as dot-product rows rather than the
// terrain shader's row-vector matrix. Convert before applying the same gate.
inline bool decodeColumnProjection(const float* columns,Projection& out,float (&rows)[16]) {
    float candidate[16];
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)candidate[r*4+c]=columns[c*4+r];
    if(!decodeProjection(candidate,out))return false;
    for(unsigned i=0;i<16;++i)rows[i]=candidate[i];
    return true;
}
