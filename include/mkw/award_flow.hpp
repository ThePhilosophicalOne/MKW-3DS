#pragma once
// Section 0x35 (Grand Prix award): page 0x3B "WinningRunFade" then page 0x3C (award results).
// Pure state machine with retail timings; the host draws, plays sound and moves the scene.
#include <cstdint>
#include <mkw/gp_award.hpp>

namespace mkw {

enum class AwardPage : uint8_t { WinningRun, Results, Done };

struct AwardFlowInput {
    bool ok = false;                // a new OK press this frame (any local pad)
    bool sceneSkippable = false;    // CameraManager fn_1_91070 (0x805A1424): still in the run
    bool runOver = false;           // CameraManager +0x28 > 0: the camera reached the podium
    bool fadeOneFinished = false;   // WinningRunFade animation 1 ended this frame
    bool fadeTwoFinished = false;   // WinningRunFade animation 2 ended this frame
};

class AwardFlow {
public:
    enum Event : uint32_t {
        EventNone = 0,
        EventFadeOut = 1,        // skip: start fade 1, stop the BGM over 30 frames (fn_1_1E8D10)
        EventPrepareCut = 2,     // skip: prepare STRM_O_AWARD_CUT (0xCC, fn_1_1E8A18)
        EventJumpToPodium = 4,   // fade 1 ended: CameraManager +0x28 = 1, vanish every kart (fn_1_9100C)
        EventStartCut = 8,       // fade 1 ended: fade 2, start AWARD_CUT (fn_1_1E87C8)
        EventShowResults = 16,   // page 0x3C replaces page 0x3B (onInit: the cup record is written)
        EventCongratsAnim = 32,  // results page frame 10: the congratulations animation
        EventGradeSe = 64,       // results page frame 180: SE_DEMO_GET_STAR_x (gpGradeSe)
        EventLeave = 128,        // OK on the results page: credits / unlock pages / menu
    };
    static constexpr unsigned SkipAfterFrame = 0x45; // fn_1_AB2C4: page frame > 0x45
    static constexpr unsigned CongratsFrame = 10;    // fn_1_AC2AC
    static constexpr unsigned BgmStopFrames = 30;
    static constexpr uint16_t StrmGpAward = 0xCA, StrmAwardCut = 0xCC, StrmAwardLose = 0xCD;

    void start(GpAwardScene scene) { *this = {}; scene_ = scene; page_ = AwardPage::WinningRun; }
    // One 60 Hz frame. Returns Event bits.
    uint32_t step(const AwardFlowInput& in);

    AwardPage page() const { return page_; }
    GpAwardScene scene() const { return scene_; }
    unsigned pageFrame() const { return pageFrame_; }
    // WinningRunFade animation now playing: 0 idle, 1 fade out, 2 fade in, 3 done.
    unsigned fadeAnimation() const { return fade_; }
    bool skipped() const { return skipped_; }
    // Music at scene start (race RSAR fn_1_201710 0x80711AC4, section 0x35).
    uint16_t sceneMusic() const { return scene_ == GpAwardScene::Lose ? StrmAwardLose : StrmGpAward; }

private:
    GpAwardScene scene_ = GpAwardScene::Win;
    AwardPage page_ = AwardPage::Done;
    unsigned pageFrame_ = 0, fade_ = 0;
    bool skipped_ = false;
};

} // namespace mkw
