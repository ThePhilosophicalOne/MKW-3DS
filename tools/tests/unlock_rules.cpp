// Host test: retail unlock rules (include/mkw/unlocks.hpp). Run tools/tests/run_unlock_tests.sh.
#include <mkw/unlocks.hpp>
#include <mkw/unlock_page.hpp>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace mkw;
using namespace mkw::unlock;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static Progress fresh() { return Progress{}; }
static void cupResult(Progress& p, unsigned cls, unsigned cup, Trophy t, GpRank rank) { p.cups[cls * 8 + cup] = {true, t, rank}; }
static void allCups(Progress& p, unsigned cls, Trophy t, GpRank rank) { for (unsigned c = 0; c < 8; ++c) cupResult(p, cls, c, t, rank); }
static std::vector<Notice> drain(Progress& p) { std::vector<Notice> v; while (auto n = activateNext(p)) v.push_back(*n); return v; }

static void starterRoster() {
    UnlockWords f{};
    unsigned characters = 0, vehicles = 0;
    for (unsigned c = 0; c < 24; ++c) characters += characterUnlocked(f, c);
    for (unsigned v = 0; v < 36; ++v) vehicles += vehicleUnlocked(f, v);
    CHECK(characters == 12 && vehicles == 18);
    for (unsigned c : {0u, 1u, 2u, 3u, 6u, 7u, 8u, 9u, 10u, 11u, 14u, 16u}) CHECK(characterUnlocked(f, c));
    CHECK(!characterUnlocked(f, 0x18) && !characterUnlocked(f, 0x1A) && characterUnlocked(f, 0x1C));
    for (unsigned cls = 0; cls < 4; ++cls) {
        unsigned cups = 0;
        for (unsigned c = 0; c < 8; ++c) cups += cupUnlocked(f, c, cls, false);
        CHECK(cups == 4);
    }
    CHECK(!mirrorUnlocked(f) && !anyMixedTypesUnlocked(f) && engineClassButtons(f) == 3);
    CHECK(!cupUnlocked(f, 8, 0, false));
}

static void bitsIndependent() {
    // Upstream's independently decoded tables (license_unlocks.hpp) agree bit for bit.
    constexpr int ch[] = {-1,-1,-1,-1,2,0,-1,-1,-1,-1,-1,-1,3,1,-1,6,-1,7,4,9,5,8,10,11};
    constexpr int ve[] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,34,37,40,35,38,41,36,39,42,-1,-1,-1,-1,-1,-1,-1,-1,-1,43,46,49,44,47,50,45,48,51};
    for (unsigned bit = 0; bit < 64; ++bit) {
        UnlockWords f{}; setFlag(f, bit);
        CHECK(flag(f, bit) && f[bit / 32] == 1u << (bit % 32));
        for (unsigned c = 0; c < 24; ++c) CHECK(characterUnlocked(f, c) == (ch[c] < 0 || unsigned(ch[c]) == bit));
        for (unsigned v = 0; v < 36; ++v) CHECK(vehicleUnlocked(f, v) == (ve[v] < 0 || unsigned(ve[v]) == bit));
        for (unsigned cls = 0; cls < 4; ++cls)
            for (unsigned cup : {2u, 3u, 6u, 7u}) {
                const unsigned base = cup == 2 ? 14 : cup == 3 ? 18 : cup == 6 ? 22 : 26;
                CHECK(cupUnlocked(f, cup, cls, false) == (base + cls == bit));
            }
        CHECK(mirrorUnlocked(f) == (bit == 52));
        CHECK(mixedTypesUnlocked(f, 0) == (bit == 30) && mixedTypesUnlocked(f, 1) == (bit == 31));
        CHECK(!mixedTypesUnlocked(f, 2) && !mixedTypesUnlocked(f, 3));
        CHECK(characterUnlocked(f, 0x1E) == (bit == 12) && characterUnlocked(f, 0x26) == (bit == 13));
    }
    // Mirror cups are their own class; the mirror flag selects it.
    UnlockWords f{}; setFlag(f, 17);
    CHECK(cupUnlocked(f, 2, 2, true) && !cupUnlocked(f, 2, 2, false) && cupUnlocked(f, 2, 3, false));
}

static void cupPageAndKinds() {
    UnlockWords f{}; setFlag(f, 15);   // Star Cup, 100cc
    CHECK(cupOpen(f, 2, true, 1, false) && !cupOpen(f, 2, true, 2, false));
    CHECK(cupOpen(f, 2, false, 2, false));   // VS / Time Trials: any class
    CHECK(!cupOpen(UnlockWords{}, 2, false, 2, false) && cupOpen(UnlockWords{}, 5, false, 0, false));
    UnlockWords m{}; setFlag(m, 29);   // Lightning, Mirror
    CHECK(cupOpen(m, 7, false, 0, false) && cupOpen(m, 7, true, 2, true) && !cupOpen(m, 7, true, 2, false));
    UnlockWords none{};
    CHECK(vehicleAllowed(none, 0, VehicleKind::Karts) && !vehicleAllowed(none, 18, VehicleKind::Karts));
    CHECK(vehicleAllowed(none, 18, VehicleKind::Bikes) && !vehicleAllowed(none, 0, VehicleKind::Bikes));
    CHECK(!vehicleAllowed(none, 9, VehicleKind::Both) && vehicleAllowed(none, 20, VehicleKind::Both));
}

static void conditions() {
    // Star Cup per class: trophies (any) in Mushroom and Flower.
    { Progress p = fresh(); cupResult(p, 0, 0, Trophy::Bronze, 8); CHECK(!conditionMet(p, 0));
      cupResult(p, 0, 1, Trophy::None, 8); CHECK(!conditionMet(p, 0));
      cupResult(p, 0, 1, Trophy::Silver, 8); CHECK(conditionMet(p, 0) && !conditionMet(p, 1));
      setFlag(p.flags, 14); CHECK(!conditionMet(p, 0)); }
    { Progress p = fresh(); cupResult(p, 3, 0, Trophy::Gold, 0); cupResult(p, 3, 1, Trophy::Gold, 0); CHECK(conditionMet(p, 3) && conditionBit(3) == 17); }
    { Progress p = fresh(); cupResult(p, 2, 2, Trophy::Bronze, 8); CHECK(conditionMet(p, 6) && conditionBit(6) == 20); }
    { Progress p = fresh(); cupResult(p, 1, 4, Trophy::Bronze, 8); cupResult(p, 1, 5, Trophy::Gold, 8); CHECK(conditionMet(p, 9) && conditionBit(9) == 23); }
    { Progress p = fresh(); cupResult(p, 0, 6, Trophy::Gold, 8); CHECK(conditionMet(p, 12) && conditionBit(12) == 26); }
    // Mirror: gold in all eight 150cc cups.
    { Progress p = fresh(); allCups(p, 2, Trophy::Gold, 8); CHECK(conditionMet(p, 16));
      cupResult(p, 2, 5, Trophy::Silver, 0); CHECK(!conditionMet(p, 16)); }
    // Gold rewards (class, cup, condition, bit).
    const unsigned golds[][4] = {{1,6,17,0},{0,7,22,4},{2,3,24,6},{0,2,31,9},{1,3,32,12},{3,7,40,36},{0,6,41,37},{2,6,42,38},
                                 {0,3,44,40},{2,7,46,42},{2,2,47,43},{3,6,50,45},{1,7,51,46},{3,2,53,48},{1,2,54,49},{3,3,57,51}};
    for (auto& g : golds) {
        Progress p = fresh(); cupResult(p, g[0], g[1], Trophy::Silver, 0); CHECK(!conditionMet(p, g[2]));
        cupResult(p, g[0], g[1], Trophy::Gold, 0); CHECK(conditionMet(p, g[2]) && conditionBit(g[2]) == g[3]);
        setFlag(p.flags, g[3]); CHECK(!conditionMet(p, g[2]));
    }
    // Dolphin Dasher is the Mirror STAR Cup (retail licence +0xBD6), not Flower (upstream's bug).
    { Progress p = fresh(); cupResult(p, 3, 1, Trophy::Gold, 0); CHECK(!conditionMet(p, 53)); }
    // Star-or-better groups (class, first cup, count, condition).
    const unsigned stars[][4] = {{0,0,4,20},{1,4,4,23},{3,0,8,26},{2,0,4,28},{0,4,4,37},{2,4,4,45},{1,0,4,52}};
    for (auto& s : stars) {
        Progress p = fresh();
        for (unsigned i = 0; i < s[2]; ++i) {
            CHECK(!conditionMet(p, s[3]));
            cupResult(p, s[0], s[1] + i, Trophy::None, i + 1 == s[2] ? 3 : 2);   // rank 3 (A) fails
        }
        CHECK(!conditionMet(p, s[3]));
        cupResult(p, s[0], s[1] + s[2] - 1, Trophy::None, 2);
        CHECK(conditionMet(p, s[3]));
    }
    // Time Trial course counts: Magikruiser 8, Birdo 16, Toadette 32.
    { Progress p = fresh();
      for (unsigned c = 0; c < 32; ++c) {
          CHECK(conditionMet(p, 48) == (c >= 8)); CHECK(conditionMet(p, 25) == (c >= 16)); CHECK(conditionMet(p, 18) == (c == 32));
          p.ttBestMs[c] = 999999;
      }
      CHECK(conditionMet(p, 18)); }
    // Expert staff ghosts: the threshold itself passes, one more millisecond fails.
    { Progress p = fresh(); p.ttBestMs[8] = 86395; CHECK(!expertGhostAppeared(p, 8)); p.ttBestMs[8] = 86394; CHECK(expertGhostAppeared(p, 8));
      CHECK(expertGhostCount(p) == 1 && conditionMet(p, 38) && !conditionMet(p, 29)); }
    { Progress p = fresh();
      const unsigned ms[32] = {100528,94160,130875,142856,137653,145620,149076,135323,86394,111786,130509,171684,177382,180007,146064,176867,
                               90909,115907,146334,172100,74072,122358,149793,156630,95213,64261,130069,163142,189228,171693,173651,101174};
      for (unsigned c = 0; c < 32; ++c) {
          const unsigned n = c;   // ghosts so far
          CHECK(conditionMet(p, 38) == (n >= 1)); CHECK(conditionMet(p, 29) == (n >= 4)); CHECK(conditionMet(p, 21) == (n >= 8));
          CHECK(conditionMet(p, 55) == (n >= 12)); CHECK(conditionMet(p, 43) == (n >= 24)); CHECK(conditionMet(p, 33) == (n >= 32));
          p.ttBestMs[c] = ms[c];
      }
      CHECK(conditionMet(p, 33)); }
    // Karts and bikes: trophies in Special and Lightning of the class.
    { Progress p = fresh(); cupResult(p, 0, 3, Trophy::Bronze, 8); CHECK(!conditionMet(p, 58));
      cupResult(p, 0, 7, Trophy::Bronze, 8); CHECK(conditionMet(p, 58) && !conditionMet(p, 59) && conditionBit(58) == 30); }
    // Dead-service counters (imported saves only) and the Galaxy route (always false).
    { Progress p = fresh(); p.wfcWins = 999; CHECK(!conditionMet(p, 19)); p.wfcWins = 1000; CHECK(conditionMet(p, 19) && conditionMet(p, 36) && conditionMet(p, 39));
      p.ghostRaceWins = 25; CHECK(conditionMet(p, 30) && conditionMet(p, 49) && !conditionMet(p, 35));
      p.races = 100000; p.battles = 0; CHECK(!conditionMet(p, 27)); }
    CHECK(conditionBit(61) == ~0u);
}

static void raceRoute() {
    const int thresholds[] = {1050,2550,1950,3150,450,3450,2850,1350,4350,750,2250,4950,1650,5100};
    for (unsigned b = 0; b < 14; ++b) CHECK(raceRouteThreshold(b) == thresholds[b]);
    for (unsigned b = 14; b < 34; ++b) CHECK(raceRouteThreshold(b) == -1);
    CHECK(raceRouteThreshold(52) == 4800 && raceRouteThreshold(37) == 300 && raceRouteThreshold(53) == -1);
    Progress p = fresh(); p.races = 299; CHECK(!raceRouteMet(p, 37)); p.races = 300; CHECK(raceRouteMet(p, 37));
    p.races = 0; p.battles = 300; CHECK(raceRouteMet(p, 37));
    // Each counter is capped at 99999 before the sum.
    p.races = 2000000; p.battles = 0; CHECK(raceRouteMet(p, 13));   // 99999 >= 5100
    setFlag(p.flags, 37); CHECK(!raceRouteMet(p, 37));
}

static void activation() {
    // Nothing pending.
    { Progress p = fresh(); CHECK(!nextSection(p) && !activateNext(p)); }
    // A 50cc Lightning gold with Special done gives (in order): Diddy Kong (22), Piranha
    // Prowler (44, 50cc Special gold), Super Blooper? no: only listed golds; 58 karts+bikes.
    { Progress p = fresh();
      cupResult(p, 0, 3, Trophy::Gold, 0); cupResult(p, 0, 7, Trophy::Gold, 0);
      CHECK(nextSection(p) == Section::Driver);
      auto v = drain(p);
      CHECK(v.size() == 3);
      CHECK(v[0].condition == 22 && v[0].bit == 4 && v[0].section == Section::Driver && v[0].headline == 0x1F42 && v[0].headlineArg[0] == 9000 + 18);
      CHECK(v[0].conditionText == 0x1FB3 && v[0].modelCharacter == 18 && v[0].modelVehicle == -1);
      CHECK(nextDelayFrames(v[0], 5) == 90 && nextVoice(v[0]) == 944);
      CHECK(v[1].condition == 44 && v[1].bit == 40 && v[1].section == Section::Vehicle && v[1].headline == 0x1F46 && v[1].headlineArg[0] == 0x238C + 11);
      CHECK(v[1].conditionText == 0x1FD3 && v[1].modelVehicle == 11 && v[1].modelCharacter == 3 && nextVoice(v[1]) == 0);
      CHECK(v[2].condition == 58 && v[2].bit == 30 && v[2].section == Section::Vehicle && v[2].headline == 0x1F4A);
      CHECK(v[2].conditionText == 0x1FE1 && v[2].modelVehicle == 0x13 && v[2].modelCharacter == 0);
      CHECK(!nextSection(p)); }
    // Cup reward: headline with class and cup name, cup icon, condition text with the class.
    { Progress p = fresh(); cupResult(p, 1, 2, Trophy::Bronze, 8);
      auto n = activateNext(p);
      CHECK(n && n->condition == 5 && n->bit == 19 && n->section == Section::Course && n->headline == 0x1F40);
      CHECK(n->headlineArg[0] == 0xD54 && n->headlineArg[1] == 0x23F3 && n->cupIcon == 3);
      CHECK(n->conditionText == 0x1FA5 && n->conditionArg == 0xD54 && n->modelCharacter == -1 && nextDelayFrames(*n, 7) == 7); }
    // Mirror via 150cc golds: Mirror icon, then the gold rewards of 150cc.
    { Progress p = fresh(); allCups(p, 2, Trophy::Gold, 8);
      auto v = drain(p);
      CHECK(v.size() >= 6 && v[0].condition == 2 && v[1].condition == 6);   // Star, then Special Cup 150cc (lower ids first)
      bool sawMirror = false;
      for (auto& n : v) if (n.bit == 52) { sawMirror = true; CHECK(n.headline == 0x1F41 && n.cupIcon == 8 && n.conditionText == 0x1FA8); }
      CHECK(sawMirror && flag(p.flags, 52)); }
    // Race-count route: only after every main condition; one bit per page; play count text.
    { Progress p = fresh(); p.races = 450; cupResult(p, 0, 0, Trophy::Gold, 0); cupResult(p, 0, 1, Trophy::Gold, 0);
      auto v = drain(p);
      CHECK(v.size() == 3);
      CHECK(v[0].condition == 0);
      CHECK(v[1].condition == -1 && v[1].bit == 4 && v[1].conditionText == 0x1FE4 && v[1].conditionNumber == 450 && v[1].section == Section::Driver);
      CHECK(v[2].bit == 37 && v[2].conditionNumber == 300 && v[2].section == Section::Vehicle && v[2].headline == 0x1F44 + 1); }
    // Race route for Mii outfits and Mirror.
    { Progress p = fresh(); p.races = 99999; p.battles = 99999;
      auto v = drain(p);
      CHECK(v.size() == 33);   // 14 characters + 18 vehicles + Mirror
      for (auto& n : v) {
          if (n.bit == 12 || n.bit == 13) CHECK(n.section == Section::Mii && n.headline == 0x1F43 && n.headlineArg[0] == (n.bit == 12 ? 0x235C : 0x235D) && nextVoice(n) == 0);
          if (n.bit == 52) CHECK(n.section == Section::Course && n.headline == 0x1F41);
      }
      for (unsigned b = 0; b + 1 < v.size(); ++b) CHECK(v[b].bit < v[b + 1].bit); }
    // An alternative condition for an unlocked reward is never shown again.
    { Progress p = fresh(); p.wfcWins = 1000; for (unsigned c = 0; c < 32; ++c) p.ttBestMs[c] = 999999;
      auto v = drain(p);
      unsigned toadette = 0; for (auto& n : v) toadette += n.bit == 1;
      CHECK(toadette == 1 && v[0].condition == 18); }
}

static void credits() {
    const auto run = [](const Progress& p, unsigned cup, unsigned cls, bool mirror, unsigned place) { return creditsAfterAward(p, cup, cls, mirror, place); };
    Progress p = fresh();
    cupResult(p, 2, 3, Trophy::Bronze, 8);
    CHECK(run(p, 3, 2, false, 3) == Credits::None);           // Lightning missing
    cupResult(p, 2, 7, Trophy::Bronze, 8);
    CHECK(run(p, 3, 2, false, 3) == Credits::Normal && run(p, 7, 2, false, 1) == Credits::Normal);
    CHECK(run(p, 3, 2, false, 4) == Credits::None && run(p, 2, 2, false, 1) == Credits::None);
    CHECK(run(p, 3, 1, false, 1) == Credits::None);           // other class
    Progress g = fresh();
    for (unsigned k = 0; k < 4; ++k) allCups(g, k, Trophy::Gold, 0);
    CHECK(run(g, 0, 0, false, 1) == Credits::True && run(g, 0, 0, false, 2) == Credits::None);
    setFlag(g.flags, BitTrueEnding);
    CHECK(run(g, 0, 0, false, 1) == Credits::None && run(g, 3, 2, true, 1) == Credits::True && run(g, 7, 0, false, 1) == Credits::True);
    CHECK(run(g, 7, 0, false, 2) == Credits::Normal);
    cupResult(g, 1, 4, Trophy::Silver, 0);
    Progress h = g; h.flags = {};
    CHECK(run(h, 3, 1, false, 1) == Credits::Normal);         // not all gold: normal on Special
    // After the award.
    Progress u = fresh(); cupResult(u, 0, 2, Trophy::Gold, 0);
    auto a = afterAward(u, 2, 0, false, 1);
    // 50cc Star gold: the Special Cup (condition 4) comes before King Boo (31).
    CHECK(a.next == AfterAward::UnlockPages && a.unlockSection == Section::Course);
    CHECK(afterAward(fresh(), 0, 0, false, 5).next == AfterAward::MainMenu);
    CHECK(afterAward(p, 3, 2, false, 3).next == AfterAward::CreditsNormal);
    // Skip and the seen bits.
    UnlockWords f{};
    CHECK(!creditsSkippable(f, Credits::Normal) && markEndingSeen(f, Credits::Normal) && flag(f, 57) && !flag(f, 56));
    CHECK(creditsSkippable(f, Credits::Normal) && !creditsSkippable(f, Credits::True) && !markEndingSeen(f, Credits::Normal));
    CHECK(markEndingSeen(f, Credits::True) && creditsSkippable(f, Credits::True) && !markEndingSeen(f, Credits::None));
}

static void selectPages() {
    UnlockWords f{};
    auto g = characterGrid(f);
    CHECK(g.rows == 2 && g.miiButtons == 0 && g.count() == 12);
    // Light starters: Baby Mario 6, Toad 8, Baby Peach 1, Koopa 14 (home slots 0, 2, 4, 5).
    // Two rows hold slots 0-3: the locked homes 1 (Baby Luigi) and 3 (Toadette) take the
    // unlocked characters whose homes lie outside the grid, in order: Baby Peach, Koopa.
    CHECK(g.button[0] == 6 && g.button[1] == 1 && g.button[2] == 8 && g.button[3] == 14);
    CHECK(g.character(4) == 0 && g.character(5) == 7 && g.character(6) == 10 && g.character(7) == 16);
    CHECK(g.character(8) == 11 && g.character(9) == 2 && g.character(10) == 9 && g.character(11) == 3 && g.character(12) == -1);
    setFlag(f, 1);   // Toadette (light home slot 3) only
    g = characterGrid(f);
    // Three rows (slots 0-5): Baby Peach and Koopa are at home now; slot 1 has nothing
    // left beyond the grid (Baby Daisy, Dry Bones locked), so it is a "?" button.
    CHECK(g.rows == 3 && g.button[3] == 13 && g.button[1] == -1 && g.button[4] == 1 && g.button[5] == 14);
    UnlockWords all{}; for (unsigned b = 0; b < 14; ++b) setFlag(all, b);
    g = characterGrid(all);
    CHECK(g.rows == 4 && g.miiButtons == 2 && g.count() == 26 && g.mii[0] == 0x1E && g.mii[1] == 0x20);
    for (unsigned c = 0; c < 3; ++c) for (unsigned k = 0; k < 8; ++k) CHECK(g.button[c * 8 + k] >= 0);
    UnlockWords miiB{}; setFlag(miiB, 13);
    g = characterGrid(miiB);
    CHECK(g.rows == 3 && g.miiButtons == 1 && g.mii[0] == 0x20);
    // Vehicles: 3 starters per kind and weight; rows follow the largest column of any class.
    UnlockWords v{};
    auto col = vehicleColumns(v, 0);
    CHECK(col.rows == 3 && col.column[0][0] == 0 && col.column[0][1] == 3 && col.column[0][2] == 6 && col.column[0][3] == -1);
    CHECK(col.column[1][0] == 18 && col.column[1][2] == 24);
    setFlag(v, 43);   // Quacker (bike, light)
    col = vehicleColumns(v, 1);
    CHECK(col.rows == 4 && col.column[1][3] == -1 && col.column[0][3] == -1);
    col = vehicleColumns(v, 0);
    CHECK(col.column[1][3] == 27);
    for (unsigned b = 34; b < 52; ++b) setFlag(v, b);
    CHECK(vehicleColumns(v, 2).rows == 6 && vehicleColumns(v, 2).column[1][5] == 35);
    UnlockWords m{}; setFlag(m, 52); CHECK(engineClassButtons(m) == 4);
    // The port's existing 4-row slot order (menu_ui.cpp within[] = {0,1,4,6,2,3,5,7}).
    const unsigned within[8] = {0, 1, 4, 6, 2, 3, 5, 7};
    for (unsigned s = 0; s < 8; ++s) CHECK(characterButton(s % 4, s / 4) == within[s]);
    CHECK(characterButton(2, 0) == 4 && characterButton(2, 1) == 5 && characterButton(1, 1) == 3);
}

static void page() {
    Progress p = fresh(); cupResult(p, 0, 7, Trophy::Gold, 0);
    UnlockPage pg;
    pg.enter(*activateNext(p));   // Diddy Kong
    CHECK(!pg.leaving());
    const auto press = pg.next();
    CHECK(press.accepted && press.voice == 944 && !press.animateVehicle && !pg.next().accepted && pg.leaving());
    CHECK(nextDelayFrames(pg.notice(), 12) == 90 && !activateNext(p));
    Progress q = fresh(); cupResult(q, 0, 2, Trophy::Bronze, 8);
    pg.enter(*activateNext(q));   // Special Cup 50cc: no model, the button's delay
    CHECK(!pg.leaving() && pg.next().accepted && nextDelayFrames(pg.notice(), 12) == 12);
}

int main() {
    page();
    starterRoster(); bitsIndependent(); cupPageAndKinds(); conditions(); raceRoute(); activation(); credits(); selectPages();
    if (failures) { std::printf("unlock_rules: %d failure(s)\n", failures); return 1; }
    std::puts("unlock_rules: starter roster, 64 independent bits, cup page, 61 conditions, race route, notice order, credits and select pages passed");
    return 0;
}
