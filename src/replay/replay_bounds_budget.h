#pragma once
#include <chrono>
#include <cstdint>

// Envelope time budgets read this clock. Tests may substitute a deterministic
// clock so that budget-limited schedules are exactly reproducible.
#ifndef NORTHLIGHT_REPLAY_BOUNDS_CLOCK
#define NORTHLIGHT_REPLAY_BOUNDS_CLOCK std::chrono::steady_clock
#endif
namespace NorthlightReplayBounds {
using BoundsClock=NORTHLIGHT_REPLAY_BOUNDS_CLOCK;
/* 0.3.137 coverage switches. Scheduling/cost only: every bound that is still
   produced is bit-identical to 0.3.136, so culling stays exactly as safe.
   FastReady: a warm cheap skin draw reads the clock once per ReadyClockStride
   draws instead of ~8 times per draw (the dominant evaluation cost measured),
   so the same cheap allowance covers roughly the whole crowd.
   BuildClockStride: cold source builds read the clock per N vertices, not per
   vertex. ResultMemo=false: the exact result memo hit ~6% (0.3.136 log) and
   copied 3.6 KiB keys on every store; never allocate or consult it. */
constexpr bool FastReady=true,ResultMemo=false;
constexpr unsigned ReadyClockStride=8,BuildClockStride=32;
// Lending is allowed only after both reserved passes have had their turn.
// Closing those passes prevents spending the same unused allowance twice.
struct AdaptiveTimeBudget {
    static constexpr std::uint64_t Cheap=280000,Heavy=70000,Build=150000,Total=500000;
    std::uint64_t cheapLimit=Cheap;
    bool reservedClosed=false;
    void reset(){cheapLimit=Cheap;reservedClosed=false;}
    void finishReservedTurns(std::uint64_t heavyUsed,std::uint64_t buildUsed){
        if(reservedClosed)return;
        reservedClosed=true;
        cheapLimit=heavyUsed>=Total||buildUsed>=Total-heavyUsed?0:Total-heavyUsed-buildUsed;
    }
    std::uint64_t lent()const{return cheapLimit>Cheap?cheapLimit-Cheap:0;}
};
}
