#pragma once
// The whole retail credits sequence on top of the GP-end agent's demo stage
// (include/mkw/demo_stage.hpp): part 1 is the ending_demo run (sections 0x39/0x3A:
// course 0x3A, game mode 0xC, every racer CPU-driven, the KMP CAME chain from camera
// 0x10, STRM_O_ENDING_PART_A 206, staff roll "EndingDemoStaffRoll", 6290 frames);
// part 2 the ending movie (0x3B/0x3C, 8060 frames); then Congratulations (0x3D/0x3E).
// START skips to Congratulations only when that ending was seen (credits::Flow).
//
// Host contract (main.cpp, wave B):
//   1. After the award's releaseRace(): run.begin(kind, skippable, field, stage). It calls
//      stage.begin(endingStage(...)) and installs the frame callback. The host loads
//      stage.config() through its race loader and starts config().music.
//   2. Part 1, every 60 Hz frame: step the scene, then stage.tick() (the callback advances
//      the credits). Parts 2 and 3: run.tick() instead.
//   3. After each frame read run.takeEvent():
//        DemoEnded  - the run has released the stage; the host runs releaseRace(), then
//                     opens the movie (stage() == Movie) or shows Congratulations
//                     (stage() == Congratulations, after a START skip);
//        MovieEnded - close the movie and its audio, show Congratulations;
//        Finished   - leave to exit(): unlock pages, the menus or the title.
//   4. START: run.start(); A on Congratulations: run.accept(); first movie frame on
//      screen: run.movieStarted().
#include <mkw/credits.hpp>
#include <mkw/demo_stage.hpp>
#include <array>
#include <cstdint>

namespace mkw::credits {

// The GP award scenario as Racedata_initCredits 0x80531070 reads it, per race-scenario slot.
struct EndingField {
    std::array<AwardRacer, GpMaxRacers> racers{};      // present / local human / finish position
    std::array<std::uint8_t, GpMaxRacers> character{}, vehicle{};
};
// endingOrder + the GP-end agent's endingStage builder (KTPT 0..n-1 in that order).
DemoStageConfig endingConfig(const EndingField&);

class Run {
public:
    enum class Event : std::uint8_t { None, DemoEnded, MovieEnded, Finished };
    void begin(Kind kind, bool skippable, const EndingField& field, DemoStage& stage);
    // Part 1: called at the end of each stage frame, after the credits stepped (the host's
    // staff-roll view then matches the 3D run's frame). Set after begin().
    void setFrameHook(void (*hook)(void* context), void* context) { hook_ = hook; hookContext_ = context; }
    // Parts 2 and 3 (part 1 is advanced by the stage's frame callback).
    void tick();
    Event takeEvent() { const Event e = event_; event_ = Event::None; return e; }
    bool start() { return flow_.start(); }
    // The port only: the ending_demo stage could not load (runDemoStage false). The stage is
    // released and part 2 follows, as after part 1 (DemoEnded).
    void abortDemo();
    bool accept() { return flow_.accept(); }
    void movieStarted() { flow_.movieStarted(); }
    Stage stage() const { return flow_.stage(); }
    Kind kind() const { return flow_.kind(); }
    const Flow& flow() const { return flow_; }
    Flow& flow() { return flow_; }
    bool active() const { return active_; }
    // Congratulations is left: where to go (credits::Flow::exit).
    Exit exit(bool unlockPending) const { return flow_.exit(unlockPending); }
private:
    static void onStageFrame(void* self, unsigned frame);
    void advance();
    Flow flow_{Kind::Normal, false};
    DemoStage* stage_ = nullptr;
    void (*hook_)(void*) = nullptr;
    void* hookContext_ = nullptr;
    Event event_ = Event::None;
    bool active_ = false;
};
}
