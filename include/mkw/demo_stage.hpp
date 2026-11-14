#pragma once
// Demo stages: the race scene runs on a demo course with CPU-driven racers, no laps, no
// checkpoints, no items. Retail uses one RaceScene for all of them (gameMode 0xB award / 0xC
// credits, RaceScene_initSLoader -> Racedata_initAwards 0x80530864 / Racedata_initCredits
// 0x80531070). Users: the Grand Prix award (winningrun_demo, course 0x37), the losing scene
// (loser_demo, 0x38) and the credits run (ending_demo, 0x3A).
//
// This module owns the stage description and its per-frame logic (frame clock, cameras,
// skip, callback). The host's race session (main.cpp) loads the stage and racers from
// DemoStageConfig, steps Kinoko and draws; see the wave B list in the GP-end Phase 2 report.
#include <array>
#include <cstddef>
#include <cstdint>
#include <mkw/gp_award.hpp>
#include <mkw/math.hpp>

namespace mkw {

enum class DemoStageKind : uint8_t { Award, Lose, Ending };
enum class DemoCamera : uint8_t { Scn0, KmpChain };

struct DemoRacer {
    bool present = false;
    bool human = false;          // a local human's racer (still CPU-driven in the stage)
    uint8_t character = 0, vehicle = 0;
    uint8_t startPoint = 0;      // KTPT index (Raceinfo_getInitialPosAndRotForPlayer 0x805362DC)
};

struct DemoStageConfig {
    DemoStageKind kind = DemoStageKind::Award;
    unsigned course = WinningRunCourse;   // retail/Kinoko course id
    unsigned gameMode = AwardGameMode;    // RaceConfig +0xB70: 0xB award, 0xC credits
    unsigned cameraMode = AwardCameraModeWin; // +0xB74: 7 win, 0xC lose
    uint16_t music = 0xCA;                // STRM started with the scene
    DemoCamera camera = DemoCamera::Scn0;
    uint16_t firstKmpCamera = 0;          // KmpChain: the first CAME entry
    unsigned racerCount = 0;
    std::array<DemoRacer, GpMaxRacers> racers{};
};

// Racedata_initAwards, cameraMode 7: slots 0-2 = standings 1-3; 0xC: the local humans by
// standing. character/vehicle/humanMask are per race-scenario index.
DemoStageConfig gpAwardStage(GpAwardScene scene, const std::array<uint8_t, GpMaxRacers>& standing,
                             const std::array<uint8_t, GpMaxRacers>& character, const std::array<uint8_t, GpMaxRacers>& vehicle,
                             uint32_t humanMask, unsigned count);
// The credits run (ending_demo, gameMode 0xC, STRM_O_ENDING_PART_A 206, the KMP CAME chain
// from camera 0x10): racers in the given order (credits::endingOrder), KTPT 0..n-1.
DemoStageConfig endingStage(const std::array<int8_t, GpMaxRacers>& order, const std::array<uint8_t, GpMaxRacers>& character,
                            const std::array<uint8_t, GpMaxRacers>& vehicle, uint32_t humanMask);

// A cooked SCN0 camera (tools/ceremony/export_scene.ts .scam: "SCAM", u32 version 1, u32
// frames, then per 60 Hz frame 13 little-endian floats: position, rotation (degrees), aim,
// twist, vertical FOV, near, far).
struct ScamFrame { float position[3], rotation[3], aim[3], twist, fovy, nearZ, farZ; };
// The view of one SCN0 frame. Every ceremony camera is the rotate type (cameraType 0): the
// camera turns by Z, then X, then Y (degrees) and looks down its -Z axis, so a positive X
// rotation tilts it up. Checked on the cooked data: demolos01 looks at loser_demo's KTPT 0,
// demowin01 at the podium (tools/tests/gp_end.cpp). The vertical FOV is used as is.
struct DemoView { Vec3 eye, target, up{0, 1, 0}; float fov = 60; };
DemoView scamView(const ScamFrame& frame);
class ScamTrack {
public:
    bool load(const void* data, size_t size);
    unsigned frames() const { return frames_; }
    // Clamped to the last frame (the 1-frame cameras are static shots).
    ScamFrame sample(unsigned frame) const;
    bool loaded() const { return values_ != nullptr; }
private:
    const float* values_ = nullptr;
    unsigned frames_ = 0;
};

// The award cameras (RKAwardDemoCamera.cpp: setup fn_1_908D4 0x805A0C88, calc fn_1_90BEC
// 0x805A0FA0, switch fn_1_9032C 0x805A06E0, skip test fn_1_91070 0x805A1424).
// Win: phase -1 = the race camera plays the course's KMP CAME chain while the karts drive
// (fn_1_9B1C8); when that chain has no next camera (fn_1_6920 returns 0) the camera sets its
// +0x50. The next frame demowin01 (123dai.brres, 641 frames) starts at frame 0 and every kart
// vanishes (phase 0); when its time passes its frame count, demowin02 (1 frame, held) starts
// (phase 1). demowin03 is loaded but this path never selects it. Time advances by 1.0 a frame.
// Lose: demolos01 (Lose.szs, the _16vs9 camera on a widescreen console) is held at frame 0 from
// the first frame. Skippable (fn_1_91070): phase -1 before any SCN0, or phase 0 with time < 400.
enum class AwardCameraPhase : int8_t { RaceChain = -1, Run = 0, Podium = 1 };
constexpr float AwardSkipWindow = 400.0f; // 0x80892070

// Podium layout (fn_1_27A3B4 0x8078A768, fn_1_27A550 0x8078A904, fn_1_27AF20 0x8078B2D4).
// Origin (0, 1224, 27850) (0x808A4930, static init fn_1_27B18C). v_stand1 sits at the origin.
// 1st (0, +140, 0), 2nd (-225, +70, 0), 3rd (+225, +70, 0), unrotated. Bystanders i = 0.. in
// gpBystanders order: even i on the +200 side, odd on the -200 side (0x808A4870, GP variant),
// both rows starting at z + 145 (0x808A4924); each bystander first adds 100 / 100 / 250
// (light / medium / heavy, 0x808A4880) to its side's z, stands there (pairs 2-3, 6-7, 10-11
// shifted a further 700 outward, 0x808A4928), turned -pi/2 (even) or +pi/2 (odd) about Y,
// then adds the same amount again. Shadow scale 0.4 / 0.5 / 0.75 by weight (0x808A4830).
struct AwardPlacement { bool used = false; float x = 0, y = 0, z = 0, yaw = 0, shadowScale = 1; };
constexpr float AwardOrigin[3] = {0.0f, 1224.0f, 27850.0f};
void awardPodiumLayout(const std::array<uint8_t, 3>& podiumWeight, unsigned podiumCount,
                       const std::array<uint8_t, GpMaxRacers>& bystanderWeight, unsigned bystanderCount,
                       std::array<AwardPlacement, 3>& podium, std::array<AwardPlacement, GpMaxRacers>& bystanders);

class DemoStage {
public:
    using FrameCallback = void (*)(void* context, unsigned frame);
    void begin(const DemoStageConfig& config) {
        *this = {}; config_ = config; active_ = true;
        if (config.kind == DemoStageKind::Lose) scn0_ = 0; // demolos01 from the first frame
    }
    void setFrameCallback(FrameCallback callback, void* context) { callback_ = callback; context_ = context; }
    // The race camera reports its KMP CAME chain ended (camera +0x50).
    void raceChainFinished() { chainDone_ = true; }
    // The cooked frame count of the current SCN0 (demowin01 / demowin02 / demolos01).
    void setCameraFrames(unsigned frames) { cameraFrames_ = frames; }
    // One 60 Hz frame after the host's scene step: cameras (fn_1_9032C, fn_1_90BEC), clock,
    // then the callback.
    void tick();
    // Award skip (page 0x3B fade end, fn_1_AB17C): phase 1 = demowin02 at frame 0, every kart
    // vanishes (fn_1_9100C 0x805A13C0).
    void jumpToPodium() { phase_ = AwardCameraPhase::Podium; scn0_ = 1; cameraTime_ = 0; cameraDone_ = false; kartsVisible_ = false; }
    void release() { active_ = false; callback_ = nullptr; context_ = nullptr; }
    bool active() const { return active_; }
    AwardCameraPhase phase() const { return phase_; }
    bool podium() const { return phase_ == AwardCameraPhase::Podium; }
    // -1 = the race camera; 0 demowin01 (lose: demolos01), 1 demowin02.
    int scn0() const { return scn0_; }
    float cameraTime() const { return cameraTime_; }
    bool skippable() const {
        if (scn0_ < 0) return phase_ == AwardCameraPhase::RaceChain;
        return phase_ == AwardCameraPhase::Run && cameraTime_ < AwardSkipWindow;
    }
    bool kartsVisible() const { return kartsVisible_; }
    // CameraManager +0x25 as the course objects' calc reads it (they run before the camera):
    // fn_1_9032C 0x805A06E0 sets it on the frame the KMP chain ends and clears it on the next
    // camera calc, which starts demowin01. So it reads true on exactly the frame between
    // raceChainFinished() and the tick that starts the SCN0 (never after a skip).
    bool cameraCut() const { return config_.kind == DemoStageKind::Award && scn0_ < 0 && chainDone_; }
    unsigned frame() const { return frame_; }
    const DemoStageConfig& config() const { return config_; }
private:
    DemoStageConfig config_{};
    FrameCallback callback_ = nullptr;
    void* context_ = nullptr;
    unsigned frame_ = 0, cameraFrames_ = 0;
    float cameraTime_ = 0;
    int scn0_ = -1;
    AwardCameraPhase phase_ = AwardCameraPhase::RaceChain;
    bool active_ = false, kartsVisible_ = true, chainDone_ = false, cameraDone_ = false;
};

} // namespace mkw
