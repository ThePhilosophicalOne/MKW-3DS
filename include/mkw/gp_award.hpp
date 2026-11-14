#pragma once
// Grand Prix end: standings, trophy, award scene choice, podium and bystanders, grid order.
// Pure: host-testable. All rules are retail PAL RMCP01.
#include <algorithm>
#include <array>
#include <cstdint>

namespace mkw {

constexpr unsigned GpMaxRacers = 12;

// ---- Standings ---------------------------------------------------------------------------
// The totals board (fn_1_34C734 0x8085CAE8) sorts with fn_1_34C8D0 (0x8085CC84): higher total
// first; on equal totals a local human ranks above a CPU; then the larger total plus this
// race's points. The row index + 1 becomes the standing (menu scenario player +0xE0).
struct GpStandingEntry { uint8_t racer = 0; uint16_t total = 0, gain = 0; bool human = false; };
inline bool gpStandingBefore(const GpStandingEntry& a, const GpStandingEntry& b) {
    int ta = a.total, tb = b.total;
    if (ta != tb) return ta > tb;
    ta += a.human; tb += b.human;
    if (ta != tb) return ta > tb;
    return ta + a.gain > tb + b.gain;
}
// standing[racer] = 1..count. With 12 racers every racer's gain differs, so no full tie
// exists; stable order keeps the racer index for the impossible case.
inline std::array<uint8_t, GpMaxRacers> gpStandings(std::array<GpStandingEntry, GpMaxRacers> rows, unsigned count) {
    count = std::min(count, GpMaxRacers);
    std::stable_sort(rows.begin(), rows.begin() + count, gpStandingBefore);
    std::array<uint8_t, GpMaxRacers> standing{};
    for (unsigned i = 0; i < count; ++i) standing[rows[i].racer % GpMaxRacers] = uint8_t(i + 1);
    return standing;
}

// ---- Award decision (RaceMenuPage_onButtonFront 0x8085A0F4, case 7, last GP race) -------
// Any local human with standing 1-3 gets the award ceremony (cameraMode 7, course 0x37
// winningrun_demo); otherwise the losing scene (cameraMode 0xC, course 0x38 loser_demo).
// Both run as gameMode 0xB in section 0x35.
enum class GpAwardScene : uint8_t { Win, Lose };
constexpr unsigned WinningRunCourse = 0x37, LoserCourse = 0x38, AwardGameMode = 0xB;
constexpr unsigned AwardCameraModeWin = 7, AwardCameraModeLose = 0xC;
inline unsigned gpBestHumanStanding(const std::array<uint8_t, GpMaxRacers>& standing, uint32_t humanMask, unsigned count) {
    unsigned best = 12; // fn_1_AC528 starts at 12
    for (unsigned i = 0; i < std::min(count, GpMaxRacers); ++i)
        if ((humanMask >> i & 1) && standing[i] && standing[i] < best) best = standing[i];
    return best;
}
inline GpAwardScene gpAwardScene(unsigned bestHumanStanding) { return bestHumanStanding < 4 ? GpAwardScene::Win : GpAwardScene::Lose; }

// ---- Trophy and texts (fn_1_ACC9C 0x805BD050, fn_1_AC870 0x805BCC24) ------------------
// 0 gold, 1 silver, 2 bronze, 3 none (4th-12th). The text is Race.bmg 0x4C3 "You got 1st
// place!", 0x4C4, 0x4C5, else 0x4C6 "Better luck next time!".
inline unsigned gpTrophy(unsigned bestHumanStanding) { return bestHumanStanding >= 1 && bestHumanStanding <= 3 ? bestHumanStanding - 1 : 3; }
inline uint16_t gpTrophyMessage(unsigned bestHumanStanding) {
    return bestHumanStanding == 1 ? 0x4C3 : bestHumanStanding == 2 ? 0x4C4 : bestHumanStanding == 3 ? 0x4C5 : 0x4C6;
}
// Header (fn_1_AC628 0x805BC9DC): cup icon + "{cup} {class}" (0x58D) with the class text
// 0x589 50cc, 0x58A 100cc, 0x58B 150cc, 0x58C Mirror. engineClass 0-2, mirror flag apart.
inline uint16_t gpClassMessage(unsigned engineClass, bool mirror) {
    return engineClass == 0 ? 0x589 : engineClass == 1 ? 0x58A : mirror ? 0x58C : 0x58B;
}
constexpr uint16_t GpAwardCupLineMessage = 0x58D;
// Demo/Trophy/<name>_trophy.brres by cup id (fn_1_ACB7C 0x805BCF30 hands the cup id over).
inline const char* gpTrophyModel(unsigned cup) {
    constexpr const char* Names[9] = {"kinoko", "flower", "star", "special", "koura", "banana", "konoha", "thunder", "battle"};
    return Names[cup < 9 ? cup : 8];
}

// ---- Racers of the scene (Racedata_initAwards 0x80530864) -------------------------------
// Win: racer slots 0-2 are the top three by standing (any type), all driven as CPUs, each on
// KTPT slot (index); Lose: the local humans by standing (one in single player).
inline std::array<int8_t, 3> gpPodium(const std::array<uint8_t, GpMaxRacers>& standing, unsigned count) {
    std::array<int8_t, 3> podium{-1, -1, -1};
    for (unsigned r = 1; r <= 3; ++r)
        for (unsigned i = 0; i < std::min(count, GpMaxRacers); ++i)
            if (standing[i] == r) { podium[r - 1] = int8_t(i); break; }
    return podium;
}
// The bystanders of the podium scene (fn_1_277E88 0x8078823C, fn_1_279F08 0x8078A2BC):
// every racer with standing 4 or lower in standing order, then a stable insertion sort that
// puts heavier weight classes first. weightClass[racer] 0 light, 1 medium, 2 heavy.
inline unsigned gpBystanders(const std::array<uint8_t, GpMaxRacers>& standing, const std::array<uint8_t, GpMaxRacers>& weightClass,
                             unsigned count, std::array<int8_t, GpMaxRacers>& out) {
    out.fill(-1);
    unsigned n = 0;
    count = std::min(count, GpMaxRacers);
    for (unsigned rank = 4; rank <= count; ++rank)
        for (unsigned i = 0; i < count; ++i)
            if (standing[i] == rank) { out[n++] = int8_t(i); break; }
    std::stable_sort(out.begin(), out.begin() + n, [&](int8_t a, int8_t b) { return weightClass[unsigned(a)] > weightClass[unsigned(b)]; });
    return n;
}
// The award model (fn_1_278F8C 0x80789340): Demo/Award/<code>.brres, or <code>3.brres for
// Daisy (0x0F), Peach (0x10) and Rosalina (0x17) on a bike (vehicle id 18 or more).
inline const char* gpAwardCharacterCode(unsigned character) {
    constexpr const char* Codes[24] = {"mr", "bpc", "wl", "kp", "bds", "ka", "bmr", "lg", "ko", "dk", "ys", "wr",
                                       "blg", "kk", "nk", "ds", "pc", "ca", "dd", "kt", "jr", "bk", "fk", "rs"};
    return character < 24 ? Codes[character] : nullptr; // Miis: their own body files
}
inline bool gpAwardBikeOutfit(unsigned character, unsigned vehicle) { return vehicle > 0x11 && (character == 0x0F || character == 0x10 || character == 0x17); }
// demowin (CHR/PAT/SRT ids 0-2) for the podium, demooth (3-5) for everyone else; each
// starts at a random frame of its clip (Util::Random::nextF32(frameCount), fn_1_2797E0).

// ---- Hand-off after the award (page 0x3C A, fn_1_AC388 0x805BC73C) ---------------------
// Retail asks fn_1_3FE10 (cup, class, mirror, best standing) for the credits first, then
// fn_1_344BF0 for the unlock pages, else section 0x41 (the menus).
struct GpAwardOutcome {
    unsigned cup = 0, engineClass = 2; bool mirror = false;
    unsigned bestStanding = 12, trophy = 3, grade = 7;
};

// ---- Grid order (Raceinfo_construct 0x805327A0, Scenario::initRace 0x8052FB90) ---------
// Race 1: start position = count - index (the human, index 0, starts last; CPU 11 on pole).
// Races 2-4: the previous race's finishing position (RaceConfig::update 0x80531CE4 copies it
// on Next). Battle: index + 1. Time Trials: 1. The award scene: KTPT index (slot < 3 win,
// < 6 lose), else KTPT 0 (Raceinfo_getInitialPosAndRotForPlayer 0x805362DC).
inline std::array<uint8_t, GpMaxRacers> gpFirstRaceGrid(unsigned count) {
    std::array<uint8_t, GpMaxRacers> start{};
    count = std::min(count, GpMaxRacers);
    for (unsigned i = 0; i < count; ++i) start[i] = uint8_t(count - i);
    return start;
}
inline unsigned awardStartPoint(unsigned slot, GpAwardScene scene) { return slot < (scene == GpAwardScene::Win ? 3u : 6u) ? slot : 0u; }

} // namespace mkw
