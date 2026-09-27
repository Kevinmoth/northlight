#!/usr/bin/env python3
"""Water mask patch of one original liquid shader: the reference oracle for
src/water/water_shader_patch.h (0.3.161), which the DLL runs at shader creation.
Renames the position output (VS) or oC0 (PS) to a spare temporary, appends two MOVs
and one DCL; rejects everything it cannot prove safe (ValueError). Pure: no MPQ,
client or StormLib, so tests/test_water_shader_patch.py needs only a C++ compiler.
"""
import struct

def register(token):return ((token>>28)&7)|((token>>8)&24), token&2047   # = extract_signatures.register
ARITY={0:0,1:2,2:3,3:3,4:4,5:3,6:2,7:2,8:3,9:3,10:3,11:3,12:3,13:3,14:2,15:2,16:2,17:3,18:4,19:2,20:3,21:3,22:3,23:3,24:3,27:2,29:0,32:3,33:3,34:4,35:2,36:2,46:2,65:1,66:3,78:2,79:2,88:4,89:3,90:4,91:2,92:2,93:5,94:3,95:3}
def reg(t,kind,index):return (t&~0x70001fff)|((kind&7)<<28)|((kind&24)<<8)|index
def dst(kind,index,mask=15):return reg(0x80000000|(mask<<16),kind,index)
def src(kind,index,swizzle=0xe4):return reg(0x80000000|(swizzle<<16),kind,index)
def parse(code):
    w=list(struct.unpack('<%dI'%(len(code)//4),code)); out=[];p=1
    if w[0] not in (0xfffe0200,0xfffe0300,0xffff0200,0xffff0300):raise ValueError('unsupported shader model')
    while p<len(w):
        op=w[p]&65535
        if op==65535:
            if w[p]!=65535 or p+1!=len(w):raise ValueError('bad END')
            return w,out,p
        n=((w[p]>>16)&32767) if op==65534 else ((w[p]>>24)&15)
        if p+n>=len(w):raise ValueError('truncated')
        out.append((p,op,w[p+1:p+1+n]));p+=n+1
    raise ValueError('missing END')
def patch(code):
    w,ops,end=parse(code);major=(w[0]>>8)&255;vertex=(w[0]>>16)==65534
    used={}; params=[];writes=[];depth=0;mask=0;declEnd=1;pos=(4,0) if vertex and major==2 else (8,0) if not vertex else None
    for p,op,a in ops:
        if op==31 and vertex and major==3 and (a[0]&15)==0:pos=register(a[1])
    if pos is None:raise ValueError('no position')
    for p,op,a in ops:
        if op==65534:continue
        if w[p]&0xf0000000:raise ValueError('predicated/coissued')
        if op==31:
            if len(a)!=2:raise ValueError('bad DCL')
            if (a[0]&15)==5 and ((a[0]>>16)&15)==7:raise ValueError('TEXCOORD7 occupied')
            k,i=register(a[1]);used.setdefault(k,set()).add(i);declEnd=p+3;continue
        if op in (81,48,47):continue
        arity=2 if op==37 and major==3 else 4 if op==37 else ARITY.get(op)
        if arity is None:raise ValueError('unsupported control/opcode %d'%op)
        if op==27:depth+=1
        if op==29:
            depth-=1
            if depth<0:raise ValueError('unbalanced LOOP')
        q=0
        for operand in range(arity):
            if q>=len(a):raise ValueError('operand count')
            t=a[q];k,i=register(t)
            if not t&0x80000000:raise ValueError('parameter marker')
            used.setdefault(k,set()).add(i);params.append(p+1+q)
            if operand==0 and op not in (27,65):
                if (k,i)==pos:
                    if depth:raise ValueError('conditional output')
                    mask|=(t>>16)&15;writes.append(p+1+q)
                elif not vertex and k in (8,9):raise ValueError('MRT/depth write')
                if t&0x2000:raise ValueError('relative dest')
            if operand==2 and 20<=op<=24:
                rows=4 if op in (20,22) else 2 if op==24 else 3
                used.setdefault(k,set()).update(range(i,i+rows))
            q+=1
            if t&0x2000:
                if q>=len(a) or register(a[q])[0] not in (3,15):raise ValueError('bad relative')
                used.setdefault(register(a[q])[0],set()).add(register(a[q])[1]);q+=1
        if q!=len(a):raise ValueError('extra operands')
    if depth or mask!=15:raise ValueError('incomplete output')
    limit=32 if major==3 else 12
    spare=next((i for i in range(limit) if i not in used.get(0,set())),None)
    if spare is None:raise ValueError('no free temporary')
    if vertex:
        kind=6;tex=7 if major==2 else next((i for i in range(12) if i not in used.get(6,set())),None)
        if tex is None or tex in used.get(6,set()):raise ValueError('no free varying')
    else:
        kind=3 if major==2 else 1;tex=7 if major==2 else next((i for i in range(10) if i not in used.get(1,set())),None)
        if tex is None or tex in used.get(kind,set()):raise ValueError('no free varying')
    for p in params:
        if register(w[p])==pos:w[p]=reg(w[p],0,spare)
    suffix=[0x02000001,dst(*pos),src(0,spare,0xe4 if vertex else 0xff),0x02000001]
    suffix += [dst(kind,tex),src(0,spare)] if vertex else [dst(8,0,1),src(kind,tex,0xff)]
    w[end:end+1]=suffix+[65535]
    if major==3 or not vertex:w[declEnd:declEnd]=[0x0200001f,0x80070005 if major==3 else 0x80000000,dst(kind,tex)]
    return struct.pack('<%dI'%len(w),*w),{'model':major,'vertex':vertex,'temp':spare,'varying':tex,'writes':len(writes),'loops':sum(op==27 for _,op,_ in ops)}
