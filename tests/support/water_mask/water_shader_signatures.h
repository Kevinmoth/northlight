#pragma once
// Test-only stand-in for the generated table that 0.3.160 embedded (deleted in 0.3.161),
// so the verbatim water_renderer_0_3_140.h still compiles. Own-authored 8-word vs_3_0/ps_3_0
// programs (sample() in tests/test_water_shaders.py) and their patch() output; the fake device
// ignores the bytes. test_water_mask_state.cpp gives the originals to the current renderer.
#include <cstdint>
struct NorthlightWaterShaderVariant { std::uint64_t hash; const DWORD* words; unsigned wordCount; bool vertex; };
static const DWORD kWaterTestOriginalVS[] = {0xfffe0300,0x0200001f,0x80000000,0xe00f0000,0x02000001,0xe00f0000,0x90e40000,0x0000ffff};
static const DWORD kWaterTestMaskVS[] = {0xfffe0300,0x0200001f,0x80000000,0xe00f0000,0x0200001f,0x80070005,0xe00f0001,0x02000001,0x800f0000,0x90e40000,0x02000001,0xe00f0000,0x80e40000,0x02000001,0xe00f0001,0x80e40000,0x0000ffff};
static const DWORD kWaterTestOriginalPS[] = {0xffff0300,0x0200001f,0x80000005,0x900f0000,0x02000001,0x800f0800,0x90e40000,0x0000ffff};
static const DWORD kWaterTestMaskPS[] = {0xffff0300,0x0200001f,0x80000005,0x900f0000,0x0200001f,0x80070005,0x900f0001,0x02000001,0x800f0000,0x90e40000,0x02000001,0x800f0800,0x80ff0000,0x02000001,0x80010800,0x90ff0001,0x0000ffff};
static const NorthlightWaterShaderVariant kWaterShaderVariants[] = {
    {UINT64_C(0x97395e736cbd495f), kWaterTestMaskVS, 17, true},
    {UINT64_C(0x9bd9798a500be7df), kWaterTestMaskPS, 17, false},
};
