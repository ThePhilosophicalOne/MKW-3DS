#pragma once
// The Kinoko side of the Grand Prix rank score (src/gp_rank.cpp).
#include <cstdint>
#include <mkw/gp_rank.hpp>

namespace mkw {
// Race start (KartSettings ctor 0x80592FC0): grandPrixMask = local humans in a Grand Prix,
// humanMask = every local human. Clears all counters.
void gpResetRankCounters(uint32_t grandPrixMask, uint32_t humanMask);
// fn_1_81498 (0x8059184C), called by Effects::Player's wall hit for a non-ghost kart: one wall
// hit unless the kart is over a zipper (status 42), or in a zipper boost (status 45) for
// fewer than 3 frames (KartMove +0x12C).
void gpCountWallHit(unsigned racer);
// The race's inputs for gpRaceScore, read after the finish.
GpRaceStats gpRaceStats(unsigned racer, const GpRaceFrames& frames, bool automaticDrift);
// ranktimeGP.krt from the Core archive (Race/Common.szs, RaceManager +0x10 in retail).
bool gpLoadRankTimes(GpRankTimes& out);
}
