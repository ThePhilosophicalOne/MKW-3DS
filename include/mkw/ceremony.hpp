#pragma once
// The host parts of the demo stages (include/mkw/demo_stage.hpp) that main.cpp's race loader
// uses: the demo AI's pads (Enemy::AIControlDemo through CpuAi Setup::demo), the cooked SCN0
// cameras, the award podium (fn_1_277E88 0x8078823C) and the award page textures
// (tools/cook_award_ui.py). main.cpp's runDemoStage loads the stage as a course and runs it.
#include <mkw/award_page.hpp>
#include <mkw/cpu_ai.hpp>
#include <mkw/demo_stage.hpp>
#include <mkw/math.hpp>
#include <mkw/race_grid.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace mkw {
class Renderer;

// A cooked SCN0 camera (models/ceremony/*.scam, tools/ceremony/export_scene.ts) in memory.
class ScamFile {
public:
    bool load(const char* path);
    void reset() { track_ = {}; data_.clear(); data_.shrink_to_fit(); }
    const ScamTrack& track() const { return track_; }
    std::size_t bytes() const { return data_.size() * sizeof(float); }
private:
    std::vector<float> data_;   // 4-byte aligned, as ScamTrack::load requires
    ScamTrack track_;
};

// Enemy::AIControlDemo for every racer of a demo stage (all racers are CPUs,
// Racedata_initAwards 0x80530864 / Racedata_initCredits 0x80531070).
class DemoDriver {
public:
    // After the stage's Kinoko scene exists: the course's ENPT route (none in loser_demo),
    // CpuAi Setup::demo with humanMask 0.
    void reset(unsigned racerCount, unsigned engineClass);
    // Before each Kinoko step: the AI, then every racer's pad and KartMove CPU steer (the
    // inputs RaceGrid::beforeStep gives a CPU).
    void beforeStep();
    const CpuAi& ai() const { return ai_; }
private:
    EnemyRoute route_;
    CpuAi ai_;
    unsigned count_ = 0;
};

// The award podium (fn_1_277E88 0x8078823C, fn_1_278F8C 0x80789340, fn_1_2797E0 0x80789B94):
// v_stand1 at the award origin, the top three on it playing demowin, everyone else in
// gpBystanders order playing demooth, each clip from a random frame drawn from the
// RaceManager's random (Util::Random::nextF32(frameCount)), with their shadow models.
class AwardPodium {
public:
    struct Member { bool used = false; uint8_t character = 0, vehicle = 0; };
    AwardPodium();
    ~AwardPodium();
    // podium[0..2] = standings 1..3; bystanders[0..count) in gpBystanders order. Needs the
    // stage's Kinoko scene (the random).
    bool load(const std::array<Member, 3>& podium, const std::array<Member, GpMaxRacers>& bystanders, unsigned bystanderCount);
    void reset();
    bool loaded() const;
    unsigned bytes() const;
    // Draw after the course's opaque models: the stand, the shadow volumes, the characters.
    void draw(Renderer& renderer, unsigned tick);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// The award scene's object effects and sounds (include/mkw/ceremony_fx.hpp): DemoEf's
// fireworks (rk_demoHanabi, SE_DEMO_HANABI), Kamifubuki's confetti (rk_kamifubuki,
// rk_kamifubukiLast) and DemoJugemu's SE_DEMO_JUGEM and rk_JugemTail. Their emitters (only those reachable
// from those roots, the race table's limits allow no more) join the race table after the
// stage's track load, their textures go behind the race table's in the effect texture array.
class RaceEffects;
class Texture;
class DemoStage;
class CeremonyEffects {
public:
    // After loadTrack (CourseEffects::load left the race table as loadRaceVfx built it).
    bool load(RaceEffects& effects, Texture* textures, unsigned capacity);
    // GPU idle, every effect gone (RaceEffects::reset): the race table back as loadRaceVfx
    // built it, the textures freed.
    void unload(RaceEffects& effects, Texture* textures);
    bool loaded() const { return loaded_; }
    // Each 60 Hz step after Kinoko's (the objects' calc), before RaceEffects::step.
    // lakituRoot: DemoJugemu's "root" joint as the last model update left it
    // (CourseObjects::demoLakituRoot), where rk_JugemTail goes. True on the step whose
    // calc starts the Lakitu's clip (the caller then calls CourseObjects::startDemoLakitu).
    bool step(RaceEffects& effects, const DemoStage& stage, const Vec3* lakituRoot);
    const char* summary() const { return summary_; }
    unsigned bytes() const { return bytes_; }
private:
    struct Handle { void* effect = nullptr; uint16_t serial = 0; uint8_t group = 0; };
    int hanabi_ = -1, kamifubuki_ = -1, kamifubukiLast_ = -1, jugemTail_ = -1;
    Handle hanabiFx_, mainFx_, lastFx_, tailFx_;
    unsigned baseTextures_ = 0, textureCount_ = 0, bytes_ = 0;
    uint16_t serial_ = 0;
    bool loaded_ = false, lakituFlagUsed_ = false;
    char summary_[160] = "ceremony fx: not loaded";
};

// The award pages' textures (AwardTextures, include/mkw/award_layout.hpp) and the
// AwardPageDrawer's batches on the top screen, as RaceFlowUi draws the race-flow layouts.
class AwardUi {
public:
    AwardUi();
    ~AwardUi();
    bool load();
    // Free only once the GPU is idle: a recorded frame may still draw them.
    void unload();
    bool loaded() const;
    unsigned bytes() const;
    void drawFade(Renderer& renderer, AwardPageDrawer& page);
    void drawPage(Renderer& renderer, AwardPageDrawer& page);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mkw
