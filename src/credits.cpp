#include <mkw/credits.hpp>

namespace mkw::credits {

StaffRoll::StaffRoll(Part part)
    : part_(part),
      table_(part == Part::Demo ? unlock_tables::StaffRollDemo : unlock_tables::StaffRollMovie),
      count_(part == Part::Demo ? 11u : 18u) {}

TextAnimation StaffRoll::step(unsigned elapsed) {
    // fn_1_BB3A8: one entry at a time; the next entry waits until the previous
    // one's out frame, then its own in frame.
    if (index_ >= count_) return TextAnimation::None;
    const auto& e = table_[index_];
    if (armed_) {
        if (e.in > elapsed) return TextAnimation::None;
        for (unsigned s = 0; s < 3; ++s) {
            Slot& slot = slots_[s];
            slot.title = e.text[s][0]; slot.name = e.text[s][1];
            slot.kind = slot.name == 0 ? Slot::Kind::Hidden : slot.title == 0 ? Slot::Kind::NameOnly : Slot::Kind::TitleAndName;
        }
        armed_ = false;
        return TextAnimation::In;
    }
    if (e.out > elapsed) return TextAnimation::None;
    armed_ = true;
    ++index_;
    return TextAnimation::Out;
}

int moviePane(unsigned t) {
    if (t - 60u < 1800u) return 0;      // 60..1859
    if (t - 1860u < 1800u) return 1;    // 1860..3659
    if (t - 3660u < 1680u) return 0;    // 3660..5339
    if (t - 5340u < 1920u) return 1;    // 5340..7259
    return -1;
}

Flow::Flow(Kind kind, bool skippable) : kind_(kind), skippable_(skippable) { enter(Stage::Demo); }

void Flow::enter(Stage s) {
    stage_ = s; frame_ = 0; offset_ = 0; movieStart_ = -1; skip_ = false; accepted_ = false;
    if (s == Stage::Demo) roll_ = StaffRoll(Part::Demo);
    if (s == Stage::Movie) roll_ = StaffRoll(Part::Movie);
}

bool Flow::tick() {
    animation_ = TextAnimation::None;
    switch (stage_) {
    case Stage::Demo:
    case Stage::Movie: {
        // fn_1_BB3A8: page frame minus the 0/1 offset (+0xA66).
        const unsigned elapsed = frame_ - offset_;
        if (skip_) { enter(Stage::Congratulations); return true; }
        if (roll_.duration() <= elapsed) { enter(stage_ == Stage::Demo ? Stage::Movie : Stage::Congratulations); return true; }
        animation_ = roll_.step(elapsed);
        ++frame_;
        return false;
    }
    case Stage::Congratulations:
        if (accepted_) { stage_ = Stage::Done; return true; }
        ++frame_;
        return false;
    case Stage::Done: return false;
    }
    return false;
}

void Flow::movieStarted() {
    if (stage_ != Stage::Movie || movieStart_ >= 0) return;
    movieStart_ = int(frame_);
    offset_ = firstFrameOffset(frame_);
}

bool Flow::start() {
    if (!skippable_ || (stage_ != Stage::Demo && stage_ != Stage::Movie)) return false;
    skip_ = true;
    return true;
}

bool Flow::accept() {
    if (stage_ != Stage::Congratulations) return false;
    accepted_ = true;
    return true;
}

Exit Flow::exit(bool unlockPending) const {
    if (kind_ == Kind::True) return Exit::Title;
    return unlockPending ? Exit::UnlockPages : Exit::MainMenu;
}

std::array<int, 12> endingOrder(const std::array<AwardRacer, 12>& racers) {
    std::array<int, 12> order;
    order.fill(-1);
    unsigned n = 0;
    for (unsigned i = 0; i < 12; ++i)
        if (racers[i].present && racers[i].local) order[n++] = int(i);
    for (unsigned position = 1; position <= 12; ++position)
        for (unsigned i = 0; i < 12 && n < 12; ++i)
            if (racers[i].present && !racers[i].local && racers[i].finishPosition == position) order[n++] = int(i);
    return order;
}
}
