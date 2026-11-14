#include <mkw/award_flow.hpp>
#include <mkw/gp_rank.hpp>

namespace mkw {

uint32_t AwardFlow::step(const AwardFlowInput& in) {
    uint32_t events = EventNone;
    switch (page_) {
    case AwardPage::WinningRun:
        // Page 0x3B calc (fn_1_AB17C 0x805BB530) runs before its input handler.
        if (scene_ == GpAwardScene::Lose) {
            // cameraMode 0xC: replaced at once by page 0x3C.
            page_ = AwardPage::Results; pageFrame_ = 0;
            return events | EventShowResults;
        }
        if (fade_ == 0 && in.runOver) {
            page_ = AwardPage::Results; pageFrame_ = 0;
            return events | EventShowResults;
        }
        if (fade_ == 1 && in.fadeOneFinished) {
            fade_ = 2;
            events |= EventJumpToPodium | EventStartCut;
        } else if (fade_ == 2 && in.fadeTwoFinished) {
            fade_ = 3;
        }
        if (fade_ == 3) {
            page_ = AwardPage::Results; pageFrame_ = 0;
            return events | EventShowResults;
        }
        // Input handler (fn_1_AB2C4 0x805BB678): a press after frame 0x45 while the camera
        // is still in the run and the fade is idle.
        if (in.ok && pageFrame_ > SkipAfterFrame && in.sceneSkippable && fade_ == 0) {
            fade_ = 1; skipped_ = true;
            events |= EventFadeOut | EventPrepareCut;
        }
        ++pageFrame_;
        break;
    case AwardPage::Results:
        // Page 0x3C calc (fn_1_AC2AC): the frame counter is the page's +0x20.
        if (pageFrame_ == CongratsFrame) events |= EventCongratsAnim;
        if (pageFrame_ == GpGradeSeFrame) events |= EventGradeSe;
        // Input handler fn_1_AC388 (0x805BC73C): no frame gate in the handler.
        if (in.ok) { page_ = AwardPage::Done; events |= EventLeave; break; }
        ++pageFrame_;
        break;
    case AwardPage::Done:
        break;
    }
    return events;
}

} // namespace mkw
