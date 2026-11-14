#pragma once
// Grand Prix rank score and grade (retail PAL RMCP01). Pure: host-testable, no Kinoko.
//
// Per race, RaceManagerPlayer::updateGpRankScore (0x805368F8) runs for each local human when
// the results board fills (fn_1_1E59C 0x8052E950, Grand Prix, cameraMode 0) and adds the
// clamped race score to the cup total (menu scenario player +0xDE, s16).
// RaceConfig::Player::computeGpRank (0x8052DAF0) turns the total and the cup points into
// 0 = three stars ... 2 = one star, 3..7 = A..E (no F: the grade is capped at 7).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mkw {

// Inputs of one race (see vendor/kinoko/source/game/kart/GpRankStats.hh for each writer).
struct GpRaceStats {
    uint32_t frames = 0;      // RaceManagerPlayer +0x2C: RaceInfo frames until the finish
    uint32_t framesIn1st = 0; // +0x30: of those, the frames in 1st place
    bool rocketStart = false; // KartSettings +0x34 +0x00
    uint32_t miniTurbos = 0;  // +0x34 +0x04
    uint32_t itemHits = 0;    // +0x38 +0x08
    uint32_t offroadFrames = 0; // +0x34 +0x08
    uint32_t wallHits = 0;    // +0x34 +0x0C
    uint32_t damage = 0;      // +0x34 +0x10
    uint32_t respawns = 0;    // +0x34 +0x14
    uint16_t aiBonus = 0;     // +0x34 +0x18
    bool wiiWheel = false;    // KPad controller type 0 (never on a 3DS)
    bool automaticDrift = false; // KPad controller +0x51
};

// RaceinfoPlayer_update (0x80535304): both counters advance on every RaceInfo frame of the
// race scene (start pan and countdown included) while the player has not finished; the
// in-1st count uses the live position, which during the countdown is the grid slot.
// RaceinfoPlayer_init (0x80534194) zeroes them. Call once per 60 Hz simulation tick, with the
// finish state as it was before this tick's lap update (retail counts the finishing frame).
struct GpRaceFrames {
    uint32_t frames = 0, framesIn1st = 0;
    void tick(unsigned position, bool finishedBefore) {
        if (finishedBefore) return;
        if (position == 1) ++framesIn1st;
        ++frames;
    }
};

// ranktimeGP.krt (Race/Common.szs): "RKGT", u32 1000, u16 32 rows, u16 header 12, then per
// retail course id 4 big-endian u16 frame counts (50cc, 100cc, 150cc, battle = 0).
// updateGpRankScore reads courseId*8 + engineClass*2 + 12; Mirror is engineClass 2.
using GpRankTimes = std::array<std::array<uint16_t, 4>, 32>;
inline bool parseGpRankTimes(const void* data, size_t size, GpRankTimes& out) {
    const auto* b = static_cast<const uint8_t*>(data);
    if (!b || size < 12 + 32 * 8 || std::memcmp(b, "RKGT", 4) != 0) return false;
    const unsigned count = unsigned(b[8]) << 8 | b[9], header = unsigned(b[10]) << 8 | b[11];
    if (count != 32 || header != 12) return false;
    for (unsigned c = 0; c < 32; ++c)
        for (unsigned e = 0; e < 4; ++e) {
            const uint8_t* p = b + header + c * 8 + e * 2;
            out[c][e] = uint16_t(p[0] << 8 | p[1]);
        }
    return true;
}
// engineClass: 0 50cc, 1 100cc, 2 150cc (Mirror passes 2, as retail's RaceConfig does).
inline uint16_t gpRankReference(const GpRankTimes& t, unsigned course, unsigned engineClass) {
    return course < 32 && engineClass < 4 ? t[course][engineClass] : 0;
}

// One race's score, in retail's integer and single-precision order, clamped to [-50, 250].
// A zero reference (no KRT) adds nothing in retail; this returns 0 for it.
inline int gpRaceScore(const GpRaceStats& s, uint16_t reference) {
    if (!reference) return 0;
    const int32_t ref = reference;
    // fctiwz(0 + 1000f * (float)(ref - frames) / (float)ref): truncation toward zero.
    const float diff = float(int32_t(uint32_t(ref) - s.frames));
    const float ratio = 1000.0f * diff / float(ref);
    uint32_t score = uint32_t(int32_t(0.0f + ratio));
    score += uint32_t(s.framesIn1st * 150u) / uint32_t(ref); // mulli, divwu
    if (s.rocketStart) score += 25;
    score += (s.miniTurbos & 0xFFFFu) * 2;
    score += (s.itemHits & 0xFFFFu) * 5;
    score -= s.offroadFrames / 3; // full 32-bit word
    score -= (s.wallHits & 0xFFFFu) * 20;
    score -= (s.damage & 0xFFFFu) * 30;
    score -= (s.respawns & 0xFFFFu) * 70;
    if (s.wiiWheel) score += 10;
    if (s.automaticDrift) score += 25;
    score += s.aiBonus;
    return std::clamp<int32_t>(int32_t(score), -50, 250);
}
// The cup total is a halfword (sth): it wraps like retail.
inline int16_t gpAddRankScore(int16_t total, int raceScore) { return int16_t(uint16_t(total) + uint16_t(raceScore)); }

// computeGpRank: RANK_SCORES 0x808900C0 and SCORES 0x808900D8.
inline unsigned gpGrade(int16_t rankScore, uint16_t points) {
    constexpr int RankScores[5] = {1000, 700, 400, 200, 0};
    constexpr unsigned Scores[4] = {60, 52, 40, 20};
    unsigned a = 5, b = 4;
    for (unsigned i = 0; i < 5; ++i) if (rankScore >= RankScores[i]) { a = i; break; }
    for (unsigned i = 0; i < 4; ++i) if (points >= Scores[i]) { b = i; break; }
    return std::min(a + b, 7u);
}
// Race.bmg: 0x58F "RANK:", grade 0x590 + g (picture-font stars F063/F062/F061, A-E F078-F07C).
constexpr uint16_t GpRankLabelMessage = 0x58F;
inline uint16_t gpGradeMessage(unsigned grade) { return uint16_t(0x590 + std::min(grade, 7u)); }
// Award results page calc (fn_1_AC2AC 0x805BC660): at page frame 180 a star grade plays
// SE_DEMO_GET_STAR_3/2/1 (0x219/0x218/0x217); A-E play nothing.
constexpr unsigned GpGradeSeFrame = 180;
inline uint16_t gpGradeSe(unsigned grade) { return grade < 3 ? uint16_t(0x219 - grade) : uint16_t(0); }

} // namespace mkw
