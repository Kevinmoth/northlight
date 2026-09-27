#pragma once
#include <string>

namespace NorthlightDxvkCompatibility {
// DXVK 2.7.1 exposes AND executes RESZ only with the AMD D3D9 identity.
// Captures read game buffers on the CPU; their mappings must be CPU-cached on
// discrete GPUs. DXVK_CONFIG is process-local and overrides only these two
// file options. All other file options and explicit inline overrides survive.
inline std::string config(const std::string& existing){
    return "d3d9.customVendorId = 1002; d3d9.cachedDynamicBuffers = True; "+existing;
}
}
