#pragma once
#include <d3d9.h>
#include <cstdint>

namespace NorthlightCaptureMetadata {
struct Request {void* buffer=nullptr;bool index=false;};
struct Info {
    std::uint64_t identity=0,revision=0;
    UINT size=0;DWORD usage=0;D3DFORMAT format={};bool known=false;
};
using Read=void(*)(const Request*,Info*,unsigned);
}
