#pragma once
#include <atomic>
// Diagnostics=0 (northlight-quality.ini): periodic/sampled log lines, diagnostic
// counters, CPU/GPU timing and the periodic MEMORY line stop (the async sampler
// keeps running: it feeds the functional memory guard). Never read by a
// rendering decision: functional budgets, schedules, memory admission and the
// mirror audit ignore it. Stored once by WorldRenderer::loadQuality() during
// device creation, before the first frame or worker; relaxed loads afterwards.
namespace NorthlightDiagnostics {
inline std::atomic<bool> enabledFlag{true};
inline bool enabled(){return enabledFlag.load(std::memory_order_relaxed);}
inline void configure(bool on){enabledFlag.store(on,std::memory_order_relaxed);}
}
