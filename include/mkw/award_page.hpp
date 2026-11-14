#pragma once
// The Grand Prix award pages as material batches (src/award_page.cpp), drawn from the cooked
// Award.szs layouts (include/mkw/award_layout.hpp, tools/cook_award_ui.py):
// page 0x3C (onInit fn_1_ABDFC 0x805BC1B0, rows fn_1_AB81C 0x805BBBD0, calc fn_1_AC2AC
// 0x805BC660) and page 0x3B's WinningRunFade (fn_1_AB008 0x805BB3BC, calc fn_1_AB17C).
#include <array>
#include <cstdint>
#include <mkw/race_flow_draw.hpp>
#include <mkw/race_flow_pose.hpp>

namespace mkw {

struct AwardRow {
    bool used = false;        // RaceConfig player type != none
    uint8_t standing = 0;     // 1..12 (the row slot is standing - 1)
    uint8_t character = 0;    // retail character id (24+ = Mii)
    bool human = false;       // local human: PlayerColor<n>P, Player, FlashPlayer
    uint8_t playerIndex = 0;  // race scenario index (PlayerColor1P..4P)
    uint16_t points = 0;      // final total (awards scenario player +0xD8)
    const char* name = nullptr; // Mii / licence name for a human; nullptr = the character name
};
struct AwardPageView {
    bool win = true;          // cameraMode 7..10: the Win layouts
    bool grandPrix = true;    // section 0x35: the rank line
    unsigned cup = 0, engineClass = 2; bool mirror = false;
    unsigned bestStanding = 12;
    unsigned grade = 7;
    std::array<AwardRow, 12> rows{};
};

class AwardPageDrawer {
public:
    // Page 0x3C onInit: every row's clip states (fn_1_ABDFC, fn_1_AB81C).
    void reset(const AwardPageView& view);
    // One 60 Hz frame of page 0x3C; frame 10 starts the congratulations fade-in.
    void step();
    void draw(FlowSink sink, void* context);
    // Page 0x3B: WinningRunFade group "Fade" animation 0 Running, 1 FadeEnter, 2 FadeExit, 3 Finish.
    // Returns true on the frame the animation ends (AwardFlowInput fadeOneFinished / fadeTwoFinished).
    bool stepFade(unsigned animation);
    void drawFade(FlowSink sink, void* context);
    // Page 0x3B's construction (fn_1_AAF10): no fade animation chosen yet.
    void resetFade() { fade_ = {}; fadeAnimation_ = 0; }
    static constexpr unsigned CongratsStartFrame = 10;
private:
    struct Row { std::array<FlowPlay, 6> plays{}; bool used = false; };
    void flush(FlowSink sink, void* context, int texture, uint8_t wrapS, uint8_t wrapT, uint32_t c0, uint32_t c1);
    AwardPageView view_{};
    std::array<Row, 12> rows_{};
    FlowPlay congrats_{}, fade_{};
    unsigned frame_ = 0, fadeAnimation_ = 0;
    struct Batch { std::array<FlowQuad, FlowDrawer::MaxQuads> quads; unsigned count = 0; FlowQuad* add() { return count < FlowDrawer::MaxQuads ? &quads[count++] : nullptr; } };
    Batch batch_{};
    FlowPose pose_{};
};

} // namespace mkw
