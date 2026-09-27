#pragma once
#include <array>
#include <chrono>
#include "diagnostics_switch.h"

namespace NorthlightStreaming {
// CPU timings only; no queries, synchronization, allocations or per-event log.
// Peaks are reported with the existing sampled frame log, not inside uploads.
struct PhaseProfile {
    /* 0.3.137 sub-phases (logged on a separate line): Owners = covered-owner
       diff, StaticCache = GpuCache::update (publish+uploads), Proofs =
       dedup proofs, Retire = render-thread snapshot retirement, Terrain =
       live terrain upload, Replay = replay GPU cache/bulk upload. */
    enum Phase { Admission, Buffers, Copies, Preload, Materials, Commit, Probes, Static,
                 Owners, StaticCache, Proofs, Retire, Terrain, Replay,
                 /* 0.3.138 splits (third log line): replay expiry releases,
                    cache bind loop, resident creation total, bulk growth
                    creation, bulk lock+copy; terrain arena update, index
                    build, index growth+copy; static publish; GI worker wait
                    for the CPU reaper before a generation deferral. */
                 ReplayExpire, ReplayBind, ReplayCreate, ReplayGrow, ReplayCopy,
                 TerrainArena, TerrainIndices, TerrainIndexUpload, StaticPublish, Count };
    std::array<double,Count> peaks{};
    struct Scope {
        using Clock=std::chrono::steady_clock;
        // Peaks only feed sampled log lines: Diagnostics=0 reads no clock.
        double& peak;const bool on=NorthlightDiagnostics::enabled();Clock::time_point start=on?Clock::now():Clock::time_point{};
        explicit Scope(double& value):peak(value){}
        ~Scope(){if(!on)return;const double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();if(elapsed>peak)peak=elapsed;}
        Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
    };
    Scope measure(Phase phase){return Scope(peaks[phase]);}
    void record(Phase phase,double milliseconds){if(milliseconds>peaks[phase])peaks[phase]=milliseconds;}
    void clear(){peaks.fill(0);}
};
}
