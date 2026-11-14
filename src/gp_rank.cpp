#include <mkw/gp_rank_runtime.hpp>
#include <game/kart/GpRankStats.hh>
#include <game/kart/KartMove.hh>
#include <game/kart/KartObjectManager.hh>
#include <game/kart/KartState.hh>
#include <game/system/ResourceManager.hh>

namespace mkw {
using namespace Kinoko;

void gpResetRankCounters(uint32_t grandPrixMask, uint32_t humanMask) {
    Kart::GpRankStatsTable::Reset(grandPrixMask, humanMask);
}

void gpCountWallHit(unsigned racer) {
    auto* stats = Kart::GpRankStatsTable::GrandPrix(racer);
    auto* manager = Kart::KartObjectManager::Instance();
    if (!stats || !manager || racer >= manager->count()) return;
    auto* kart = manager->object(racer);
    const auto& status = kart->status();
    if (status.onBit(Kart::eStatus::OverZipper)) return;
    if (status.onBit(Kart::eStatus::ZipperBoost) && kart->move()->zipperBoostTimer() < 3) return;
    ++stats->wallHits;
}

GpRaceStats gpRaceStats(unsigned racer, const GpRaceFrames& frames, bool automaticDrift) {
    const auto& k = Kart::GpRankStatsTable::Get(racer);
    GpRaceStats s;
    s.frames = frames.frames;
    s.framesIn1st = frames.framesIn1st;
    s.rocketStart = k.rocketStart;
    s.miniTurbos = k.miniTurbos;
    s.itemHits = k.itemHits;
    s.offroadFrames = k.offroadFrames;
    s.wallHits = k.wallHits;
    s.damage = k.damage;
    s.respawns = k.respawns;
    s.aiBonus = k.aiBonus;
    s.wiiWheel = false; // no Wii Wheel on a 3DS: retail gives the +10 only to controller type 0
    s.automaticDrift = automaticDrift;
    return s;
}

bool gpLoadRankTimes(GpRankTimes& out) {
    auto* resources = System::ResourceManager::Instance();
    if (!resources) return false;
    size_t size = 0;
    const void* file = resources->getFile("ranktimeGP.krt", &size, System::ArchiveId::Core);
    return file && parseGpRankTimes(file, size, out);
}

} // namespace mkw
