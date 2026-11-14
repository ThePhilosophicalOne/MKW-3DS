#pragma once
// The retail unlock notification page (page 0xCD in sections 0x90-0x93), as a
// state machine the menus drive. Retail (PAL): onActivate fn_1_343D50 0x80854104
// sets the bit and saves at once; the Next button's front handler fn_1_3440C8
// 0x8085447C plays the character voice (or the vehicle model's animation) and
// changes section after 90 frames when a model is shown (else the button's own
// delay, nextDelayFrames): the next pending unlock section, or section 0x41 (the
// menus, main menu page). The port's MenuTransition holds the page that long
// (uiMenuExitHold) from the press. B (fn_1_34439C) and START (MenuPage default, vtable +0x7C, empty)
// do nothing. Music: section BGM 0x73 STRM_O_LEVEL_OPEN_FAN (fn_1_1218B4).
#include <mkw/unlocks.hpp>
#include <cstdint>

namespace mkw::unlock {
inline constexpr unsigned UnlockMusic = 0x73;   // STRM_O_LEVEL_OPEN_FAN

class UnlockPage {
public:
    // Page activation with the notice from activateNext (already saved).
    void enter(const Notice& n) { notice_ = n; leaving_ = false; }
    const Notice& notice() const { return notice_; }
    // A on Next. Returns false when already leaving (retail ignores further presses:
    // the page's leave flag +0x42C is set once). voice: the sound to play now (0 none);
    // animateVehicle: start the vehicle model's state-3 animation.
    struct Press { bool accepted = false; std::uint16_t voice = 0; bool animateVehicle = false; };
    Press next() {
        if (leaving_) return {};
        leaving_ = true;
        return {true, nextVoice(notice_), notice_.modelVehicle >= 0};
    }
    bool leaving() const { return leaving_; }
private:
    Notice notice_{};
    bool leaving_ = false;
};
}
