// Host test: retail credits flow (include/mkw/credits.hpp). Run tools/tests/run_unlock_tests.sh.
#include <mkw/credits.hpp>
#include <mkw/credits_run.hpp>
#include <cstdio>
using namespace mkw;
using namespace mkw::credits;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static void staffRoll() {
    StaffRoll demo(Part::Demo);
    CHECK(demo.duration() == 6290);
    for (unsigned f = 0; f < 420; ++f) CHECK(demo.step(f) == TextAnimation::None);
    CHECK(demo.step(420) == TextAnimation::In && demo.showing());
    CHECK(demo.slots()[0].kind == Slot::Kind::TitleAndName && demo.slots()[0].title == 8910 && demo.slots()[0].name == 8911);
    CHECK(demo.slots()[1].kind == Slot::Kind::Hidden && demo.slots()[2].kind == Slot::Kind::Hidden);
    for (unsigned f = 421; f < 720; ++f) CHECK(demo.step(f) == TextAnimation::None);
    CHECK(demo.step(720) == TextAnimation::Out && !demo.showing() && demo.entry() == 1);
    CHECK(demo.step(779) == TextAnimation::None && demo.step(780) == TextAnimation::In);
    // Count the in/out pairs over the whole part: 11 entries.
    StaffRoll whole(Part::Demo);
    unsigned ins = 0, outs = 0;
    for (unsigned f = 0; f < 6290; ++f) { const auto a = whole.step(f); ins += a == TextAnimation::In; outs += a == TextAnimation::Out; }
    CHECK(ins == 11 && outs == 11);
    StaffRoll movie(Part::Movie);
    ins = outs = 0;
    bool nameOnly = false;
    for (unsigned f = 0; f < 8060; ++f) {
        const auto a = movie.step(f);
        ins += a == TextAnimation::In; outs += a == TextAnimation::Out;
        if (a == TextAnimation::In && movie.slots()[0].kind == Slot::Kind::NameOnly) { nameOnly = true; CHECK(f == 7680 && movie.slots()[0].name == 8992); }
    }
    // The last entry (7680..9000) is still on screen when part 2 ends at 8060.
    CHECK(ins == 17 && outs == 16 && nameOnly && movie.showing());
}

static void panes() {
    CHECK(moviePane(59) == -1 && moviePane(60) == 0 && moviePane(1859) == 0 && moviePane(1860) == 1);
    CHECK(moviePane(3659) == 1 && moviePane(3660) == 0 && moviePane(5339) == 0 && moviePane(5340) == 1);
    CHECK(moviePane(7259) == 1 && moviePane(7260) == -1 && moviePane(9000) == -1);
}

static void flow() {
    {   // Full normal run, no skip.
        Flow f(Kind::Normal, false);
        CHECK(f.stage() == Stage::Demo && !f.start());
        unsigned frames = 0;
        while (!f.tick()) ++frames;
        CHECK(frames == 6290 && f.stage() == Stage::Movie && f.pane() == -1);
        for (unsigned i = 0; i < 3; ++i) CHECK(!f.tick());
        f.movieStarted();   // movie shows at page frame 3: offset 1
        unsigned movieFrames = 0;
        while (!f.tick()) ++movieFrames;
        CHECK(movieFrames == 8061 - 3 && f.stage() == Stage::Congratulations);
        CHECK(!f.tick() && f.accept() && f.tick() && f.stage() == Stage::Done);
        CHECK(f.exit(true) == Exit::UnlockPages && f.exit(false) == Exit::MainMenu);
    }
    {   // Pane timeline counts from the movie start.
        Flow f(Kind::True, false);
        while (!f.tick()) {}
        f.movieStarted();
        for (unsigned i = 0; i < 60; ++i) { CHECK(f.pane() == -1); f.tick(); }
        CHECK(f.pane() == 0);
        CHECK(f.exit(true) == Exit::Title);
    }
    {   // Skip only when seen; from either part straight to Congratulations.
        Flow f(Kind::Normal, true);
        for (unsigned i = 0; i < 100; ++i) f.tick();
        CHECK(f.start() && f.tick() && f.stage() == Stage::Congratulations && !f.start());
        Flow g(Kind::True, true);
        while (!g.tick()) {}
        CHECK(g.stage() == Stage::Movie && g.start() && g.tick() && g.stage() == Stage::Congratulations);
    }
    CHECK(demoSection(Kind::Normal) == 0x39 && movieSection(Kind::True) == 0x3C && congratulationsSection(Kind::True) == 0x3E);
    CHECK(creditsOf(Kind::True) == unlock::Credits::True);
}

static void order() {
    std::array<AwardRacer, 12> r{};
    for (unsigned i = 0; i < 12; ++i) r[i] = {true, i == 4, 12 - i};   // slot 4 is the human
    const auto o = endingOrder(r);
    CHECK(o[0] == 4);
    // Then the CPUs by finish position 1..12: slot 11 (1st), 10, 9, ..., skipping 4.
    CHECK(o[1] == 11 && o[2] == 10 && o[7] == 5 && o[8] == 3 && o[11] == 0);
    std::array<AwardRacer, 12> few{};
    few[0] = {true, false, 2}; few[1] = {true, true, 3}; few[2] = {true, false, 1};
    const auto p = endingOrder(few);
    CHECK(p[0] == 1 && p[1] == 2 && p[2] == 0 && p[3] == -1);
}

static EndingField gpField() {
    EndingField f;
    for (unsigned i = 0; i < 12; ++i) {
        f.racers[i] = {true, i == 0, i == 0 ? 2u : (i < 2 ? 1u : i + 1)};   // human 2nd, slot 1 won, slot i>=2 at i+1
        f.character[i] = std::uint8_t(i * 2); f.vehicle[i] = std::uint8_t(i + 18);
    }
    return f;
}

static void run() {
    {   // Part 1 on the demo stage, then the movie, then Congratulations.
        DemoStage stage;
        Run r;
        r.begin(Kind::Normal, false, gpField(), stage);
        const auto& c = stage.config();
        CHECK(stage.active() && c.kind == DemoStageKind::Ending && c.course == 0x3A && c.gameMode == 0xC);
        CHECK(c.music == DemoMusic && c.camera == DemoCamera::KmpChain && c.firstKmpCamera == 0x10 && c.racerCount == 12);
        CHECK(c.racers[0].human && c.racers[0].character == 0 && c.racers[0].vehicle == 18 && c.racers[0].startPoint == 0);
        CHECK(c.racers[1].character == 2 && !c.racers[1].human && c.racers[1].startPoint == 1);   // finish 1
        CHECK(c.racers[2].character == 4 && c.racers[11].character == 22 && c.racers[11].startPoint == 11);
        unsigned frames = 0;
        for (;;) {
            stage.tick();
            ++frames;
            const auto e = r.takeEvent();
            if (e != Run::Event::None) { CHECK(e == Run::Event::DemoEnded); break; }
            CHECK(frames < 7000);
            if (frames >= 7000) break;
        }
        CHECK(frames == 6291 && !stage.active() && r.stage() == Stage::Movie);
        stage.tick();   // released: no more callbacks
        CHECK(r.takeEvent() == Run::Event::None && r.flow().frame() == 0);
        r.tick(); r.tick();
        r.movieStarted();
        unsigned movie = 2;
        for (;;) { r.tick(); ++movie; if (r.takeEvent() == Run::Event::MovieEnded) break; if (movie > 9000) break; }
        CHECK(movie == 8062 && r.stage() == Stage::Congratulations);
        r.tick(); CHECK(r.takeEvent() == Run::Event::None);
        CHECK(r.accept());
        r.tick(); CHECK(r.takeEvent() == Run::Event::Finished && !r.active());
        CHECK(r.exit(false) == Exit::MainMenu);
    }
    {   // START skip during part 1 (ending seen before): straight to Congratulations.
        DemoStage stage;
        Run r;
        r.begin(Kind::True, true, gpField(), stage);
        for (unsigned i = 0; i < 100; ++i) stage.tick();
        CHECK(r.takeEvent() == Run::Event::None && r.start());
        stage.tick();
        CHECK(r.takeEvent() == Run::Event::DemoEnded && r.stage() == Stage::Congratulations && !stage.active());
        CHECK(r.exit(true) == Exit::Title);
    }
    {   // Not seen: START does nothing.
        DemoStage stage;
        Run r;
        r.begin(Kind::Normal, false, gpField(), stage);
        CHECK(!r.start());
    }
    {   // Staff-roll animation is exposed per frame during part 1.
        DemoStage stage;
        Run r;
        r.begin(Kind::Normal, false, gpField(), stage);
        unsigned in = 0;
        for (unsigned i = 0; i < 421; ++i) { stage.tick(); in += r.flow().animation() == TextAnimation::In; }
        CHECK(in == 1 && r.flow().roll().slots()[0].title == 8910);
    }
    {   // The host's frame hook runs after each part-1 frame's credits step (the same frame's In).
        DemoStage stage;
        Run r;
        r.begin(Kind::Normal, false, gpField(), stage);
        struct Seen { Run* run; unsigned calls = 0, in = 0; } seen{&r};
        r.setFrameHook([](void* c) { auto& s = *static_cast<Seen*>(c); ++s.calls; s.in += s.run->flow().animation() == TextAnimation::In; }, &seen);
        for (unsigned i = 0; i < 421; ++i) stage.tick();
        CHECK(seen.calls == 421 && seen.in == 1);
    }
}

int main() {
    staffRoll(); panes(); flow(); order(); run();
    if (failures) { std::printf("credits_flow: %d failure(s)\n", failures); return 1; }
    std::puts("credits_flow: staff roll timing, movie panes, credits flow and skip, ending_demo order passed");
    return 0;
}
