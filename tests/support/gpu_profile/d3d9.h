#pragma once
#include <cstdint>
using DWORD=uint32_t;using HRESULT=int32_t;using BOOL=int32_t;
constexpr HRESULT S_OK=0,S_FALSE=1,E_FAIL=int32_t(0x80004005u);constexpr BOOL FALSE=0;
#define FAILED(x) ((x)<0)
constexpr DWORD D3DISSUE_BEGIN=2,D3DISSUE_END=1;
enum D3DQUERYTYPE{D3DQUERYTYPE_TIMESTAMP,D3DQUERYTYPE_TIMESTAMPFREQ,D3DQUERYTYPE_TIMESTAMPDISJOINT};
struct IDirect3DDevice9;
struct IDirect3DQuery9 {IDirect3DDevice9* d;D3DQUERYTYPE type;uint64_t stamp=0;bool ended=false;
    HRESULT Issue(DWORD);HRESULT GetData(void*,DWORD,DWORD);unsigned Release();};
struct IDirect3DDevice9 {bool ready=false,failCreate=false,failIssue=false,failRead=false,disjoint=false;uint64_t time=0,frequency=1000000;unsigned live=0,creates=0,reads=0,issues=0;
    HRESULT CreateQuery(D3DQUERYTYPE,IDirect3DQuery9**);};
