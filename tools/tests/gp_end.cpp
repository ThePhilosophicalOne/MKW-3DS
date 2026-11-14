// Host test: Grand Prix rank, standings, award flow and demo stage (retail PAL rules).
// Run tools/tests/run_gp_end_tests.sh. All data is synthetic (no disc tables).
#include <mkw/award_flow.hpp>
#include <mkw/ceremony_fx.hpp>
#include <mkw/demo_stage.hpp>
#include <mkw/gp_award.hpp>
#include <mkw/gp_rank.hpp>
#include <mkw/nw4r_ef.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace mkw;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static void grade() {
    CHECK(gpGrade(1000, 60) == 0 && gpGrade(999, 60) == 1);
    CHECK(gpGrade(700, 60) == 1 && gpGrade(699, 60) == 2);
    CHECK(gpGrade(400, 60) == 2 && gpGrade(200, 60) == 3 && gpGrade(0, 60) == 4 && gpGrade(-1, 60) == 5);
    CHECK(gpGrade(1000, 59) == 1 && gpGrade(1000, 52) == 1 && gpGrade(1000, 51) == 2);
    CHECK(gpGrade(1000, 40) == 2 && gpGrade(1000, 20) == 3 && gpGrade(1000, 19) == 4);
    CHECK(gpGrade(-1, 0) == 7 && gpGrade(-500, 19) == 7); // capped: E, never F
    CHECK(gpGradeMessage(0) == 0x590 && gpGradeMessage(7) == 0x597 && gpGradeMessage(9) == 0x597);
    CHECK(gpGradeSe(0) == 0x219 && gpGradeSe(1) == 0x218 && gpGradeSe(2) == 0x217 && gpGradeSe(3) == 0);
}

static void raceScore() {
    GpRaceStats s;
    s.frames = 10000;
    CHECK(gpRaceScore(s, 10000) == 0);
    s.frames = 9000; CHECK(gpRaceScore(s, 10000) == 100);
    s.frames = 11000; CHECK(gpRaceScore(s, 10000) == -50);  // -100 clamped
    s.frames = 10500; CHECK(gpRaceScore(s, 10000) == -50);
    s.frames = 10001; CHECK(gpRaceScore(s, 10000) == 0);    // -0.1 truncates toward zero
    s.frames = 10000; s.framesIn1st = 10000; CHECK(gpRaceScore(s, 10000) == 150);
    s.framesIn1st = 6666; CHECK(gpRaceScore(s, 10000) == 99); // unsigned division truncates
    s.framesIn1st = 0; s.rocketStart = true; CHECK(gpRaceScore(s, 10000) == 25);
    s.rocketStart = false; s.miniTurbos = 0x10003; CHECK(gpRaceScore(s, 10000) == 6); // u16 mask
    s.miniTurbos = 0; s.itemHits = 4; CHECK(gpRaceScore(s, 10000) == 20);
    s.itemHits = 0; s.offroadFrames = 29; s.aiBonus = 10; CHECK(gpRaceScore(s, 10000) == 1); // -9 +10
    s.offroadFrames = 0; s.aiBonus = 0; s.wallHits = 1; s.damage = 1; s.respawns = 1; s.automaticDrift = true; s.frames = 0;
    CHECK(gpRaceScore(s, 10000) == 250); // 1000 -20 -30 -70 +25 clamped
    CHECK(gpRaceScore(s, 0) == 0);
    CHECK(gpAddRankScore(32767, 1) == -32768);
    GpRaceFrames f;
    f.tick(12, false); f.tick(1, false); f.tick(1, true);
    CHECK(f.frames == 2 && f.framesIn1st == 1);
}

static void rankTimes() {
    std::vector<uint8_t> b(12 + 32 * 8, 0);
    std::memcpy(b.data(), "RKGT", 4); b[4] = 0; b[5] = 0; b[6] = 3; b[7] = 0xE8; b[8] = 0; b[9] = 32; b[10] = 0; b[11] = 12;
    b[12 + 5 * 8 + 2 * 2] = 0x12; b[12 + 5 * 8 + 2 * 2 + 1] = 0x34;
    GpRankTimes t{};
    CHECK(parseGpRankTimes(b.data(), b.size(), t));
    CHECK(gpRankReference(t, 5, 2) == 0x1234 && gpRankReference(t, 32, 2) == 0 && gpRankReference(t, 5, 4) == 0);
    b[0] = 'X'; CHECK(!parseGpRankTimes(b.data(), b.size(), t));
    b[0] = 'R'; b[9] = 31; CHECK(!parseGpRankTimes(b.data(), b.size(), t));
    CHECK(!parseGpRankTimes(b.data(), 20, t));
}

static void standings() {
    std::array<GpStandingEntry, GpMaxRacers> rows{};
    for (unsigned i = 0; i < 12; ++i) rows[i] = {uint8_t(i), uint16_t(10), uint16_t(i), i == 0};
    rows[3].total = 20; rows[3].gain = 1;
    const auto st = gpStandings(rows, 12);
    CHECK(st[3] == 1);  // highest total
    CHECK(st[0] == 2);  // a human beats every CPU on equal totals
    CHECK(st[11] == 3 && st[10] == 4 && st[1] == 12); // then this race's points
    GpStandingEntry a{1, 30, 3, false}, b{2, 30, 5, false};
    CHECK(gpStandingBefore(b, a) && !gpStandingBefore(a, b));
    std::array<uint8_t, GpMaxRacers> s2{};
    for (unsigned i = 0; i < 12; ++i) s2[i] = uint8_t(i + 1);
    CHECK(gpBestHumanStanding(s2, 1u << 2, 12) == 3 && gpAwardScene(3) == GpAwardScene::Win);
    CHECK(gpBestHumanStanding(s2, 1u << 3, 12) == 4 && gpAwardScene(4) == GpAwardScene::Lose);
    CHECK(gpTrophy(1) == 0 && gpTrophy(3) == 2 && gpTrophy(4) == 3 && gpTrophy(12) == 3);
    CHECK(gpTrophyMessage(1) == 0x4C3 && gpTrophyMessage(2) == 0x4C4 && gpTrophyMessage(3) == 0x4C5 && gpTrophyMessage(9) == 0x4C6);
    CHECK(gpClassMessage(0, false) == 0x589 && gpClassMessage(1, false) == 0x58A && gpClassMessage(2, false) == 0x58B && gpClassMessage(2, true) == 0x58C);
    CHECK(std::strcmp(gpTrophyModel(0), "kinoko") == 0 && std::strcmp(gpTrophyModel(7), "thunder") == 0);
    const auto podium = gpPodium(s2, 12);
    CHECK(podium[0] == 0 && podium[1] == 1 && podium[2] == 2);
    std::array<uint8_t, GpMaxRacers> weight{};
    for (unsigned i = 0; i < 12; ++i) weight[i] = uint8_t(i % 3);
    std::array<int8_t, GpMaxRacers> by{};
    CHECK(gpBystanders(s2, weight, 12, by) == 9);
    // ranks 4..12 = racers 3..11; heavy (2) first: 5, 8, 11, then medium 4, 7, 10, then light 3, 6, 9
    CHECK(by[0] == 5 && by[1] == 8 && by[2] == 11 && by[3] == 4 && by[6] == 3 && by[8] == 9 && by[9] == -1);
    CHECK(std::strcmp(gpAwardCharacterCode(0), "mr") == 0 && std::strcmp(gpAwardCharacterCode(23), "rs") == 0 && !gpAwardCharacterCode(24));
    CHECK(gpAwardBikeOutfit(0x10, 18) && !gpAwardBikeOutfit(0x10, 17) && !gpAwardBikeOutfit(0, 30));
}

static void grid() {
    const auto g = gpFirstRaceGrid(12);
    CHECK(g[0] == 12 && g[11] == 1 && g[5] == 7);
    const auto g3 = gpFirstRaceGrid(3);
    CHECK(g3[0] == 3 && g3[2] == 1 && g3[3] == 0);
    CHECK(awardStartPoint(2, GpAwardScene::Win) == 2 && awardStartPoint(3, GpAwardScene::Win) == 0);
    CHECK(awardStartPoint(5, GpAwardScene::Lose) == 5 && awardStartPoint(6, GpAwardScene::Lose) == 0);
}

static void flow() {
    AwardFlow f;
    f.start(GpAwardScene::Win);
    AwardFlowInput in; in.sceneSkippable = true;
    CHECK(f.sceneMusic() == 0xCA);
    for (unsigned i = 0; i <= 0x45; ++i) { in.ok = i == 0x45; CHECK(f.step(in) == 0); }
    in.ok = true;
    uint32_t e = f.step(in); // page frame 0x46
    CHECK((e & AwardFlow::EventFadeOut) && (e & AwardFlow::EventPrepareCut) && f.fadeAnimation() == 1);
    in.ok = false; CHECK(f.step(in) == 0);
    in.fadeOneFinished = true; e = f.step(in);
    CHECK((e & AwardFlow::EventJumpToPodium) && (e & AwardFlow::EventStartCut) && f.fadeAnimation() == 2);
    in.fadeOneFinished = false; in.fadeTwoFinished = true; e = f.step(in);
    CHECK((e & AwardFlow::EventShowResults) && f.page() == AwardPage::Results && f.skipped());
    in = {};
    for (unsigned i = 0; i <= 180; ++i) {
        e = f.step(in);
        CHECK(((e & AwardFlow::EventCongratsAnim) != 0) == (i == 10));
        CHECK(((e & AwardFlow::EventGradeSe) != 0) == (i == 180));
    }
    in.ok = true; CHECK((f.step(in) & AwardFlow::EventLeave) && f.page() == AwardPage::Done);
    // Not skippable: the press is ignored; the run ends by itself.
    f.start(GpAwardScene::Win);
    in = {}; for (unsigned i = 0; i < 100; ++i) f.step(in);
    in.ok = true; CHECK(f.step(in) == 0);
    in.ok = false; in.runOver = true; CHECK(f.step(in) == AwardFlow::EventShowResults);
    // Lose: results at once, AWARD_LOSE music.
    f.start(GpAwardScene::Lose); in = {};
    CHECK(f.sceneMusic() == 0xCD && f.step(in) == AwardFlow::EventShowResults);
}

static void demo() {
    std::array<uint8_t, GpMaxRacers> standing{}, ch{}, ve{};
    for (unsigned i = 0; i < 12; ++i) { standing[i] = uint8_t(12 - i); ch[i] = uint8_t(i); ve[i] = uint8_t(20 + i); }
    auto w = gpAwardStage(GpAwardScene::Win, standing, ch, ve, 1u, 12);
    CHECK(w.course == 0x37 && w.gameMode == 0xB && w.cameraMode == 7 && w.music == 0xCA && w.racerCount == 3);
    CHECK(w.racers[0].character == 11 && w.racers[1].character == 10 && w.racers[2].character == 9 && w.racers[2].startPoint == 2);
    auto l = gpAwardStage(GpAwardScene::Lose, standing, ch, ve, 1u, 12);
    CHECK(l.course == 0x38 && l.cameraMode == 0xC && l.music == 0xCD && l.racerCount == 1 && l.racers[0].human && l.racers[0].character == 0);
    std::array<int8_t, GpMaxRacers> order{};
    for (unsigned i = 0; i < 12; ++i) order[i] = int8_t(11 - i);
    auto e = endingStage(order, ch, ve, 1u);
    CHECK(e.course == 0x3A && e.gameMode == 0xC && e.music == 206 && e.camera == DemoCamera::KmpChain && e.firstKmpCamera == 0x10);
    CHECK(e.racerCount == 12 && e.racers[0].character == 11 && e.racers[11].human && e.racers[11].startPoint == 11);
    alignas(4) uint8_t buf[12 + 2 * 13 * 4]{};
    std::memcpy(buf, "SCAM", 4); buf[4] = 1; buf[8] = 2;
    float v[26]; for (int i = 0; i < 26; ++i) v[i] = float(i);
    std::memcpy(buf + 12, v, sizeof v);
    ScamTrack t;
    CHECK(t.load(buf, sizeof buf) && t.frames() == 2);
    CHECK(t.sample(0).position[0] == 0 && t.sample(1).position[0] == 13 && t.sample(9).farZ == 25 && t.sample(0).fovy == 10);
    buf[4] = 2; CHECK(!t.load(buf, sizeof buf));
    DemoStage d; unsigned calls = 0;
    d.begin(w); d.setFrameCallback([](void* c, unsigned) { ++*static_cast<unsigned*>(c); }, &calls);
    d.tick(); d.tick(); CHECK(calls == 2 && d.frame() == 2 && d.kartsVisible());
    d.jumpToPodium(); CHECK(d.podium() && !d.kartsVisible() && d.scn0() == 1);
    d.release(); d.tick(); CHECK(calls == 2 && !d.active());
    // Award cameras: the KMP chain, then demowin01 (skippable below 400), then demowin02.
    d.begin(w);
    CHECK(d.scn0() == -1 && d.skippable() && d.kartsVisible());
    d.tick(); CHECK(d.scn0() == -1);
    d.raceChainFinished(); d.setCameraFrames(641); d.tick();
    CHECK(d.scn0() == 0 && d.phase() == AwardCameraPhase::Run && !d.kartsVisible() && d.skippable());
    for (int i = 0; i < 400; ++i) d.tick();
    CHECK(d.cameraTime() == 400.f && !d.skippable());
    for (int i = 0; i < 241; ++i) d.tick();
    CHECK(d.scn0() == 0 && d.cameraTime() == 641.f);
    d.tick(); CHECK(d.scn0() == 0); // time passes 641: held, finished
    d.tick(); CHECK(d.scn0() == 1 && d.podium() && !d.skippable());
    // Lose: demolos01 from the first frame, never skippable.
    d.begin(l); CHECK(d.scn0() == 0 && !d.skippable());
    // The podium layout (fn_1_27A550 / fn_1_27AF20).
    std::array<AwardPlacement, 3> pod{}; std::array<AwardPlacement, GpMaxRacers> by2{};
    std::array<uint8_t, GpMaxRacers> bw{}; bw[0] = 2; bw[1] = 0; bw[2] = 1; bw[3] = 1;
    awardPodiumLayout({0, 1, 2}, 3, bw, 4, pod, by2);
    CHECK(pod[0].y == 1364.f && pod[1].x == -225.f && pod[1].y == 1294.f && pod[2].x == 225.f && pod[2].shadowScale == 0.75f);
    CHECK(by2[0].x == 200.f && by2[0].z == 27850.f + 145.f + 250.f && by2[0].yaw < 0);
    CHECK(by2[1].x == -200.f && by2[1].z == 27850.f + 145.f + 100.f && by2[1].yaw > 0);
    CHECK(by2[2].x == 900.f && by2[2].z == 27850.f + 145.f + 500.f + 100.f && by2[3].x == -900.f && !by2[4].used);
    // SCN0 rotate cameras (scamView): frames as cooked from Lose.szs / Award.szs.
    auto angleTo = [](const DemoView& v, Vec3 p) {
        const Vec3 f = normalized(v.target - v.eye), d = normalized(p - v.eye);
        return std::acos(std::fmin(1.f, dot(f, d))) * 57.29578f;
    };
    ScamFrame lose{};
    const float lp[3] = {-11897.1f, 6010.1f, 52633.3f}, lr[3] = {-8.6f, -37.1f, 0.3f};
    std::memcpy(lose.position, lp, sizeof lp); std::memcpy(lose.rotation, lr, sizeof lr); lose.fovy = 58.4f;
    const DemoView lv = scamView(lose);
    CHECK(angleTo(lv, {-10604.f, 5488.f, 51922.f}) < 30.f); // loser_demo KTPT 0, the losing kart
    CHECK(std::fabs(dot(lv.up, normalized(lv.target - lv.eye))) < 1e-4f && std::fabs(length(lv.up) - 1.f) < 1e-4f && lv.up.y > 0.9f && lv.fov == 58.4f);
    ScamFrame run{};
    const float wp[3] = {12.5f, 1300.f, 29346.8f}, wr[3] = {4.3f, 0.5f, 0.f};
    std::memcpy(run.position, wp, sizeof wp); std::memcpy(run.rotation, wr, sizeof wr); run.fovy = 33.8f;
    const DemoView wv = scamView(run); // demowin01 frame 320: the podium ahead
    CHECK(angleTo(wv, {AwardOrigin[0], AwardOrigin[1] + 140.f, AwardOrigin[2]}) < 33.8f * .5f);
    CHECK(wv.target.y > wv.eye.y); // a positive X rotation tilts up
}

#ifndef MKW_SOURCE_ROOT
#define MKW_SOURCE_ROOT "."
#endif
static bool readFile(const char* path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    out.resize(size_t(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

static void ceremonyFx() {
    // DemoEf (fn_1_1AF4D4): RaceManager +0x20 > 0x527.
    CHECK(!demoFireworksOn(0x527) && demoFireworksOn(0x528));
    // Kamifubuki (fn_1_1AF640) by camera phase and the +0x25 cut.
    ConfettiCalls c = demoConfetti(-1, false);
    CHECK(!c.createMain && c.moveMain && !c.fadeMain && !c.createLast);
    c = demoConfetti(-1, true); CHECK(c.createMain && c.moveMain && !c.createLast);
    c = demoConfetti(0, false); CHECK(!c.createMain && c.moveMain);
    c = demoConfetti(1, false); CHECK(!c.createMain && !c.moveMain && c.fadeMain && c.createLast && c.moveLast);
    // DemoJugemu (fn_1_24E5D8): the cut, or the VS award flag (cameraMode 8..10).
    CHECK(!demoLakituStart(false, demoLakituVsFlag(7)) && demoLakituStart(true, demoLakituVsFlag(7)));
    CHECK(demoLakituVsFlag(8) && demoLakituVsFlag(10) && !demoLakituVsFlag(0xC));
    // CameraManager +0x25 as the objects read it: the frame between the chain's end and demowin01.
    std::array<uint8_t, GpMaxRacers> st{}, ch{}, ve{};
    for (unsigned i = 0; i < GpMaxRacers; ++i) st[i] = uint8_t(i + 1);
    DemoStage d;
    d.begin(gpAwardStage(GpAwardScene::Win, st, ch, ve, 1u, 12));
    CHECK(!d.cameraCut());
    d.tick(); CHECK(!d.cameraCut());
    d.raceChainFinished(); CHECK(d.cameraCut() && d.scn0() < 0);
    d.tick(); CHECK(!d.cameraCut() && d.scn0() == 0);
    d.begin(gpAwardStage(GpAwardScene::Win, st, ch, ve, 1u, 12));
    d.jumpToPodium(); d.raceChainFinished(); CHECK(!d.cameraCut());
    d.begin(gpAwardStage(GpAwardScene::Lose, st, ch, ve, 1u, 12)); d.raceChainFinished(); CHECK(!d.cameraCut());
    // The trophy (fn_1_23C96C, fn_1_23CA9C, fn_1_23CB4C).
    CHECK(trophyColourFrame(1) == 0.f && trophyColourFrame(2) == 5.f && trophyColourFrame(3) == 9.f);
    CHECK(!std::strcmp(trophyEffect(1), "rk_trophyGold") && !std::strcmp(trophyEffect(3), "rk_trophyCopper") && !trophyEffect(4));
    CHECK(!std::strcmp(TrophyCups[3], "special") && !std::strcmp(TrophyCups[8], "battle"));
    AwardTrophyMotion t;
    t.start();
    auto sinRad = [](float r) { return std::sin(double(r)); };
    t.calc(sinRad);
    CHECK(t.sliding && std::fabs(t.x - (700.f - 700.f * float(std::sin(1.8 * 0.0174533)))) < 1e-3f);
    CHECK(std::fabs(t.spin - (-30.f + 29.8f * float(std::sin(1.8 * 0.0174533)))) < 1e-4f);
    unsigned frames = 1;
    while (t.sliding && frames < 200) { t.calc(sinRad); ++frames; }
    CHECK(!t.sliding && t.x == 0.f && frames >= 49 && frames <= 51);
    CHECK(std::fabs(t.spin + 0.2f) < 1e-3f);
    const float r = t.rotation; t.calc(sinRad);
    CHECK(t.x == 0.f && std::fabs(t.rotation - (r + t.spin * 0.0174533f)) < 1e-6f);
    // The REFX subsets CeremonyEffects appends (the cooked award stage tables, when present).
    std::vector<uint8_t> table, out;
    std::vector<uint16_t> used;
    const char* hanabi[] = {"rk_demoHanabi"};
    const char* confetti[] = {"rk_kamifubuki", "rk_kamifubukiLast"};
    auto names = [](const std::vector<uint8_t>& b, uint32_t& emitters, uint32_t& textures) {
        std::memcpy(&emitters, b.data() + 8, 4); std::memcpy(&textures, b.data() + 12, 4);
        std::vector<std::string> n;
        for (uint32_t i = 0; i < emitters; ++i) n.emplace_back(reinterpret_cast<const char*>(b.data() + 20 + textures * 40 + i * 64));
        return n;
    };
    const std::string fx = std::string(MKW_SOURCE_ROOT) + "/sd-card/3ds/mkw-3ds/textures/fx/";
    if (readFile((fx + "winningrun_demo_rk_demo/winningrun_demo_rk_demo_0.refx").c_str(), table)) {
        CHECK(refxSubset(table, hanabi, 1, out, used));
        uint32_t e = 0, x = 0;
        const auto n = names(out, e, x);
        CHECK(e == 13 && x == used.size() && x > 0 && n[0].rfind("rk_demoHanabi", 0) == 0);
        for (const auto& s : n) CHECK(s.rfind("rk_demoCup", 0) != 0 && s != "test");
        const char* missing[] = {"rk_nothing"};
        CHECK(!refxSubset(table, missing, 1, out, used));
    } else std::printf("gp_end: cooked award effect tables not found, subset check skipped\n");
    if (readFile((fx + "winningrun_demo_rk_kamifubuki/winningrun_demo_rk_kamifubuki_0.refx").c_str(), table)) {
        CHECK(refxSubset(table, confetti, 2, out, used));
        uint32_t e = 0, x = 0;
        const auto n = names(out, e, x);
        CHECK(e == 14 && x == used.size());
        bool shortOnes = false;
        for (const auto& s : n) shortOnes |= s.find("_short") != std::string::npos;
        CHECK(!shortOnes); // rk_kamifubukiRoad*_short / rk_kamifubuki1_short are not reachable
    }
}

// NW4R emitter form 5 (EmitterFormCube::Emission, PAL fn_8003A660).
static void cubeForm() {
    using mkw::ef::CubeForm;
    using mkw::ef::Random;
    auto lcg = [](uint32_t s, int n) { for (int i = 0; i < n; ++i) s = s * 0x343fdu + 0x269ec3u; return s; };
    // The award firework seeds: 18000 x 1 x 5800, hollow 0 -> 1e-5 (the slab path, 4 rands).
    const float seeds[6] = {18000.f, 1.f, 5800.f, 0.f, 0.f, 0.f};
    CubeForm cube(seeds, 0.f);
    CHECK(cube.hollow == CubeForm::MinSize && cube.solid == 1.f - CubeForm::MinSize && cube.sy == 1.f);
    Random r; r.srand(0x1234u);
    unsigned slabs[3] = {};
    for (int i = 0; i < 2000; ++i) {
        const uint32_t before = r.seed;
        // The slab the volumes pick (first rand), as fn_8003A660 computes them.
        Random peek; peek.srand(before);
        const float u = float(peek.rand()) / 65536.f;
        mkw::Vec3 p, n;
        cube.sample(r, p, n);
        CHECK(r.seed == lcg(before, 4));
        CHECK(std::fabs(p.x) <= 18000.f && std::fabs(p.y) <= 1.f && std::fabs(p.z) <= 5800.f);
        const float a = cube.a, b = cube.b, c = cube.c;
        const float ax = std::fabs(a), by = std::fabs(b), cz = std::fabs(c);
        // The normal is the dominant unit coordinate's axis (y first, then x, then z).
        if (b >= 0 && b >= ax && b >= cz) CHECK(n.y == 1.f && n.x == 0.f && n.z == 0.f);
        else if (b < 0 && by >= ax && by >= cz) CHECK(n.y == -1.f);
        else if (a >= 0 && a >= by && a >= cz) CHECK(n.x == 1.f);
        else if (a < 0 && ax >= by && ax >= cz) CHECK(n.x == -1.f);
        else CHECK(std::fabs(n.z) == 1.f);
        // The slab: |y| outside the hollow core (y slab), else |z| (z slab), else |x|.
        if (by > CubeForm::MinSize) ++slabs[0];
        else if (cz > CubeForm::MinSize) ++slabs[1];
        else { ++slabs[2]; CHECK(ax >= CubeForm::MinSize * 0.999f); }
        CHECK(u >= 0.f && u < 1.f);
    }
    CHECK(slabs[0] > 1990); // the y slab holds (1 - 1e-5) of the shell's volume
    // Hollow 100 % (params[3] 100): a face by rand % 6, 3 rands, one coordinate exactly +-1.
    const float faces[6] = {10.f, 20.f, 30.f, 100.f, 0.f, 0.f};
    CubeForm shellOnly(faces, 0.f);
    CHECK(shellOnly.hollow == 1.f);
    for (int i = 0; i < 600; ++i) {
        const uint32_t before = r.seed;
        Random peek; peek.srand(before);
        const uint32_t face = peek.rand() % 6;
        mkw::Vec3 p, n;
        shellOnly.sample(r, p, n);
        CHECK(r.seed == lcg(before, 3));
        const float want[6][3] = {{9, 9, 1}, {9, 9, -1}, {9, 1, 9}, {9, -1, 9}, {1, 9, 9}, {-1, 9, 9}};
        const float got[3] = {shellOnly.a, shellOnly.b, shellOnly.c};
        for (int k = 0; k < 3; ++k) if (want[face][k] != 9) CHECK(got[k] == want[face][k]);
    }
    // Diffusion: the normal leans toward the position (axis = off-axis length / tan).
    CubeForm lean(seeds, 0.5f);
    Random q; q.srand(77u);
    mkw::Vec3 p, n;
    lean.sample(q, p, n);
    if (n.x == 0.f && n.z == 0.f) CHECK(false); // y axis: x and z must be the position's
    if (std::fabs(lean.b) >= std::fabs(lean.a) && std::fabs(lean.b) >= std::fabs(lean.c)) {
        CHECK(n.x == p.x && n.z == p.z);
        const float m = std::fabs(p.x) > std::fabs(p.z) ? std::fabs(p.x) : std::fabs(p.z);
        const float k = (1.f / float(std::tan(double(m * 0.5f)))) * std::sqrt(p.x * p.x + p.z * p.z);
        CHECK(std::fabs(n.y - (lean.b >= 0 ? k : -k)) <= 1e-3f * std::fabs(k));
    }
    // 0x8003A79C: a diffusion of pi moves off it by 3.7450702e-7.
    CubeForm pi(seeds, 3.1415927410125732f);
    CHECK(pi.diffusion == 3.1415927410125732f - 3.7450702e-07f);
    // Sizes are clamped at 1e-5 without fabs (a negative size becomes 1e-5).
    const float negative[6] = {-5.f, 0.f, 2.f, 50.f, 0.f, 0.f};
    CubeForm clamp(negative, 0.f);
    CHECK(clamp.sx == CubeForm::MinSize && clamp.sy == CubeForm::MinSize && clamp.sz == 2.f && clamp.hollow == 0.01f * 50.f);
}

int main() {
    grade(); raceScore(); rankTimes(); standings(); grid(); flow(); demo(); ceremonyFx(); cubeForm();
    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("gp_end: all tests passed\n");
    return 0;
}
