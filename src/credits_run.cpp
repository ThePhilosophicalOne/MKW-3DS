#include <mkw/credits_run.hpp>

namespace mkw::credits {

DemoStageConfig endingConfig(const EndingField& field) {
    const auto order = endingOrder(field.racers);
    std::array<std::int8_t, GpMaxRacers> o{};
    std::uint32_t humans = 0;
    for (unsigned k = 0; k < GpMaxRacers; ++k) o[k] = std::int8_t(order[k]);
    for (unsigned i = 0; i < GpMaxRacers; ++i)
        if (field.racers[i].present && field.racers[i].local) humans |= 1u << i;
    return endingStage(o, field.character, field.vehicle, humans);
}

void Run::begin(Kind kind, bool skippable, const EndingField& field, DemoStage& stage) {
    flow_ = Flow(kind, skippable);
    stage_ = &stage;
    event_ = Event::None;
    active_ = true;
    hook_ = nullptr;
    hookContext_ = nullptr;
    stage.begin(endingConfig(field));
    stage.setFrameCallback(&Run::onStageFrame, this);
}

void Run::onStageFrame(void* self, unsigned) {
    auto& run = *static_cast<Run*>(self);
    run.advance();
    if (run.hook_) run.hook_(run.hookContext_);
}

void Run::abortDemo() {
    if (flow_.stage() != Stage::Demo) return;
    flow_.endDemo();
    if (stage_) { stage_->release(); stage_ = nullptr; }
    event_ = Event::DemoEnded;
}

void Run::tick() {
    if (flow_.stage() == Stage::Demo) return;   // part 1 runs on the stage's callback
    advance();
}

void Run::advance() {
    if (!active_) return;
    const Stage before = flow_.stage();
    if (!flow_.tick()) return;
    if (before == Stage::Demo) {
        // Sections 0x39/0x3A end: the ending_demo scene goes (retail section change).
        if (stage_) { stage_->release(); stage_ = nullptr; }
        event_ = Event::DemoEnded;
    } else if (before == Stage::Movie) event_ = Event::MovieEnded;
    else if (flow_.stage() == Stage::Done) { event_ = Event::Finished; active_ = false; }
}
}
