// Host test: the locked select pages and the unlock / credits page input of MenuState
// (src/menu_state.cpp). Run tools/tests/run_unlock_tests.sh.
#include <mkw/menu_ui.hpp>
#include <mkw/menu_transition.hpp>
#include <mkw/menu_animations.hpp>
#include <cstdio>
using namespace mkw;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static UiInput press() { UiInput k; k.accept = true; return k; }

int main() {
    MenuState m;
    // Class page: three classes without Mirror, the START switch inert.
    m.selection.mode = UiMode::GrandPrix;
    m.enter(UiPage::EngineClass);
    CHECK(m.count() == 3);
    UiInput sw; sw.switchType = true;
    m.input(sw);
    CHECK(!m.selection.mixedEngineTypes);
    unlock::setFlag(m.unlocks, 52);
    m.enter(UiPage::EngineClass);
    CHECK(m.count() == 4);
    // 50cc karts and bikes unlocked: the switch acts, 50cc becomes mixed, 100cc stays bikes.
    unlock::setFlag(m.unlocks, 30);
    m.input(sw);
    CHECK(m.selection.mixedEngineTypes);
    m.selection.engineClass = UiEngineClass::CC50; CHECK(!m.kartsOnly());
    m.selection.engineClass = UiEngineClass::CC100; CHECK(m.bikesOnly());

    // Character page with the starter roster: 2 visual columns per band, 12 buttons.
    MenuState c;
    c.enter(UiPage::Character);
    CHECK(c.count() == 12 && c.columns() == 2);
    // Row 0 of the light band: Baby Mario, then Baby Peach filling Baby Luigi's home slot.
    CHECK(c.characterAt(0) == 4 && c.characterAt(1) == 5);
    // Row 1: Toad (home 2) and Koopa filling Toadette's home slot (3).
    CHECK(c.characterAt(2) == 6 && c.characterAt(3) == 7);
    CHECK(c.characterAt(4) == 0 && c.characterAt(12) == -1);
    unlock::setFlag(c.unlocks, 1);   // Toadette: three columns, a "?" in the light band
    c.enter(UiPage::Character);
    CHECK(c.count() == 18 && c.columns() == 3);
    CHECK(c.characterAt(1) == -1 && !c.selectable(1) && c.characterAt(4) == 22);
    // A "?" never takes the focus: moving right from slot 0 skips it.
    c.cursor = 0; UiInput right; right.right = true; c.input(right);
    CHECK(c.cursor == 2);
    // Accepting a "?" does nothing.
    c.cursor = 1; c.input(press());
    CHECK(c.page == UiPage::Character && !c.unavailable);

    // Vehicles: three rows, kart and bike columns; a locked one is not offered.
    MenuState v;
    v.selection.mode = UiMode::VS; v.selection.driver = 4;   // Baby Mario (light)
    v.enter(UiPage::Vehicle);
    CHECK(v.count() == 6 && v.columns() == 2 && v.vehicleRows() == 3);
    CHECK(v.vehicleAt(0) == 0 && v.vehicleAt(1) == 3 && v.vehicleAt(2) == 1 && v.vehicleAt(5) == 5);
    unlock::setFlag(v.unlocks, 43);   // Quacker: four rows; kart column row 3 is "?"
    v.enter(UiPage::Vehicle);
    CHECK(v.count() == 8 && v.vehicleAt(6) == -1 && v.vehicleAt(7) == 9);
    v.selection.mode = UiMode::GrandPrix; v.selection.engineClass = UiEngineClass::CC50;
    v.enter(UiPage::Vehicle);
    CHECK(v.count() == 4 && v.columns() == 1 && v.vehicleAt(3) == -1);

    // Cups: Grand Prix by class, VS by any class; a locked cup is skipped and refused.
    MenuState u;
    u.selection.mode = UiMode::GrandPrix; u.selection.engineClass = UiEngineClass::CC150;
    u.enter(UiPage::Cup);
    CHECK(u.cupSelectable(0) && u.cupSelectable(4) && !u.cupSelectable(2) && !u.cupSelectable(7));
    u.cursor = 2; u.input(press());
    CHECK(u.page == UiPage::Cup);
    unlock::setFlag(u.unlocks, 14);   // Star Cup 50cc
    CHECK(!u.cupSelectable(2));
    u.selection.mode = UiMode::VS;
    CHECK(u.cupSelectable(2));

    // Title: one picture until a licence saw the true ending.
    MenuState t;
    t.enter(UiPage::Title);
    CHECK(t.titleVariant == 0);
    t.enter(UiPage::Mode); t.titleVariants = true; t.enter(UiPage::Title);
    CHECK(t.titleVariant < 5);

    // Unlock page: A starts the page's leave once (no action, no page change); B does
    // nothing; credits START and Congratulations A set the flags main.cpp reads.
    MenuState n;
    n.enter(UiPage::Unlock);
    CHECK(n.input(press()) == UiAction::None && n.unlockPage.leaving() && n.page == UiPage::Unlock);
    UiInput back; back.back = true;
    CHECK(n.input(back) == UiAction::None && n.page == UiPage::Unlock);
    UiInput start; start.start = true;
    n.enter(UiPage::CreditsDemo);
    CHECK(n.input(start) == UiAction::None && n.creditsSkip && !n.creditsDone);
    CHECK(n.input(press()) == UiAction::None && !n.creditsDone);
    n.enter(UiPage::Congratulations);
    CHECK(n.input(press()) == UiAction::None && n.creditsDone);
    // Next's section change (fn_1_3440C8): 90 frames in total with a model, else the
    // button's own delay; the transition holds the page that long from the press.
    MenuState h;
    h.enter(UiPage::Unlock);
    unlock::Notice model;
    model.modelCharacter = 3;
    h.unlockPage.enter(model);
    CHECK(uiMenuExitHold(h) == 90.f);
    h.unlockPage.enter(unlock::Notice{});
    CHECK(uiMenuExitHold(h) == uiButtonConfirmFrames(uiSelectedMenuControl(h)));
    // Licence select: a pending reward on the card opens the unlock page instead of the main menu.
    MenuState l;
    l.licenses[1].occupied = true;
    l.unlockPendingAt[1] = true;
    l.selection.license = 1;
    l.enter(UiPage::License);
    CHECK(l.cursor == 1 && l.input(press()) == UiAction::SelectLicense && l.page == UiPage::Unlock);
    l.unlockPendingAt[1] = false;
    l.enter(UiPage::License);
    CHECK(l.input(press()) == UiAction::SelectLicense && l.page == UiPage::MainMenu);

    if (failures) { std::printf("menu_locks: %d failure(s)\n", failures); return 1; }
    std::puts("menu_locks: class page, character grid and \"?\" buttons, vehicle columns, cups, title variant and unlock/credits input passed");
    return 0;
}
