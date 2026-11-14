#pragma once
// Retail unlock rules (StaticR, PAL addresses; full write-up and the unlock
// table: findings/r1_unlocks_credits_plan.md). Pure C++, host-testable.
//   checks       characterIsUnlocked 0x8054FED8, fn_1_3FD18 0x805500CC (vehicle),
//                fn_1_30CC00 0x8081CFB4 (vehicle kind), fn_1_3F890 0x8054FC44 (cup),
//                fn_1_2D5A14 0x807E5DC8 (cup page), isFastStaffGhostUnlocked 0x80550468
//   conditions   fn_1_350B78 0x80860F2C (61, priority order), fn_1_352704 0x80862AB8 (bit),
//                fn_1_3524F0 0x808628A4 / fn_1_3525D4 0x80862988 (race-count route)
//   unlock page  fn_1_344BF0 0x80854FA4 (next section), fn_1_343D50 0x80854104 (activate),
//                fn_1_3443A0 0x80854754 (condition text), fn_1_34445C 0x80854810 and
//                fn_1_344A4C 0x80854E00 (headline), fn_1_34478C 0x80854B40 (models),
//                fn_1_3440C8 0x8085447C (Next)
//   credits      fn_1_3FE10 0x805501C4, award exit fn_1_AC388 0x805BC73C,
//                Congratulations fn_1_BC334 0x805CC6E8, skip fn_1_BAE10 0x805CB1C4
//   select pages fn_1_2D21F4 0x807E25A8 + CtrlMenuCharacterSelect_getCharacterIdForButton
//                0x807E3CAC (characters), VehicleSelectPage_onInit 0x80844D68 +
//                fn_1_33515C 0x80845510 (vehicles), fn_1_32F0F4 0x8083F4A8 / fn_1_32F814
//                0x8083FBC8 (class page)
// Character and vehicle ids are retail ids (Mii 0x18..0x29); engine class 0 50cc,
// 1 100cc, 2 150cc, 3 Mirror; cups in retail order 0 Mushroom .. 7 Lightning.
// The unlock default (locked like retail) is NewLicenseStartsUnlocked in save_store.hpp.
#include <mkw/save_store.hpp>
#include <array>
#include <cstdint>
#include <optional>

namespace mkw::unlock {

inline constexpr unsigned BitMixed50 = 30, BitMixed100 = 31, BitMirror = 52;
inline constexpr unsigned BitTrueEnding = 56, BitNormalEnding = 57;   // retail 0x38 / 0x39
inline constexpr unsigned ConditionCount = 61, RaceRouteBits = 58;

bool flag(const UnlockWords&, unsigned bit);       // UnlockFlags_get 0x8054994C
void setFlag(UnlockWords&, unsigned bit);          // UnlockFlags_set 0x805498FC

bool isMii(unsigned character);
bool isMiiOutfitA(unsigned character);             // fn_1_30CC74 0x8081D028
bool isMiiOutfitB(unsigned character);             // fn_1_30CCA8 0x8081D05C
bool characterUnlocked(const UnlockWords&, unsigned character);
bool vehicleUnlocked(const UnlockWords&, unsigned vehicle);
enum class VehicleKind : std::uint8_t { Karts = 0, Bikes = 1, Both = 2 };
bool vehicleAllowed(const UnlockWords&, unsigned vehicle, VehicleKind);
inline unsigned vehicleWeight(unsigned vehicle) { return vehicle % 3; }
bool cupUnlocked(const UnlockWords&, unsigned cup, unsigned engineClass, bool mirror);
// The cup select page: a Grand Prix needs the cup for the chosen class; VS and
// Time Trials need it in any of 50cc, 100cc, 150cc or Mirror.
bool cupOpen(const UnlockWords&, unsigned cup, bool grandPrix, unsigned engineClass, bool mirror);
bool mirrorUnlocked(const UnlockWords&);
// The class page's START switch to "karts and bikes": 50cc bit 30, 100cc bit 31.
bool mixedTypesUnlocked(const UnlockWords&, unsigned engineClass);
bool anyMixedTypesUnlocked(const UnlockWords&);

// What the conditions read from the active licence (SaveStore fills it, see
// unlockProgress() in unlock_save.hpp).
struct Progress {
    UnlockWords flags{};
    std::array<CupRecord, 32> cups{};          // [engineClass * 8 + cup]
    std::array<std::uint32_t, 32> ttBestMs{};  // rank-1 Time Trial time per retail course id, 0 = none
    std::uint32_t races = 0, battles = 0;      // RKPD 0xB4 / 0xB8 (licence +0x9298 / +0x9294)
    // Counters fed only by Nintendo WFC (licence +0x927C + 0x9284) and Mario Kart
    // Channel ghost races (+0x928C): dead services, so non-zero only in an imported save.
    std::uint32_t wfcWins = 0, ghostRaceWins = 0;
    bool galaxySave = false;                   // bss_68+0x34: N/A-3DS, always false
    const CupRecord& cup(unsigned engineClass, unsigned cupId) const { return cups[(engineClass & 3) * 8 + (cupId & 7)]; }
};
bool hasTrophy(const CupRecord&);              // valid and trophy != None
bool hasGold(const CupRecord&);
bool starOrBetter(const CupRecord&);           // valid and rank <= 2 (one star or better)

bool expertGhostAppeared(const Progress&, unsigned course);   // retail course id
unsigned expertGhostCount(const Progress&);                     // fn_1_352498 0x8086284C
unsigned timeTrialCourses(const Progress&);

bool conditionMet(const Progress&, unsigned condition);
unsigned conditionBit(unsigned condition);
int raceRouteThreshold(unsigned bit);          // -1: no race-count route for this bit
bool raceRouteMet(const Progress&, unsigned bit);

// Retail sections of the unlock pages.
enum class Section : std::uint8_t { Driver = 0x90, Vehicle = 0x91, Mii = 0x92, Course = 0x93 };
std::optional<Section> nextSection(const Progress&);

// One unlock page (page 0xCD). Message ids are Present_U Menu.bmg.
struct Notice {
    Section section = Section::Course;
    unsigned bit = 0;
    int condition = -1;                 // -1: race-count route
    std::uint16_t headline = 0;         // 0x1F40..0x1F4B in the message window
    std::uint16_t headlineArg[2]{};     // the headline's message arguments (0 = none)
    std::uint16_t conditionText = 0;    // page message (+0x3E4): 0x1FA4.. or 0x1FE4
    std::uint16_t conditionArg = 0;     // its message argument (class name 0xD53+class)
    std::uint32_t conditionNumber = 0;  // its number argument (play count threshold)
    int cupIcon = -1;                   // cup id for CupIcon's cup_icon_set_p, 8 = Mirror icon
    int modelCharacter = -1;            // DriverModel character, or the driver shown on the vehicle
    int modelVehicle = -1;              // VehicleModel vehicle
};
// fn_1_343D50: picks the first pending reward, sets its bit in progress.flags
// and returns the page. The caller saves at once (SaveStore::setUnlocked +
// saveNow, retail requestSaveLicenses). nullopt when nothing is pending.
std::optional<Notice> activateNext(Progress&);
// fn_1_3440C8: frames between Next and the section change (90 with a model).
unsigned nextDelayFrames(const Notice&, unsigned buttonDelay);
// fn_1_3589A0: the sound played on Next for a character reward (0: none).
std::uint16_t nextVoice(const Notice&);

// Credits after a Grand Prix award (fn_1_3FE10). place: the human's final standing.
enum class Credits : std::uint8_t { None, Normal, True };
Credits creditsAfterAward(const Progress&, unsigned cup, unsigned engineClass, bool mirror, unsigned place);
// fn_1_AC388, Grand Prix only (VS/Battle awards go to the menus): what follows the award results.
enum class AfterAward : std::uint8_t { CreditsNormal, CreditsTrue, UnlockPages, MainMenu };
struct AfterAwardResult { AfterAward next = AfterAward::MainMenu; Section unlockSection = Section::Course; };
AfterAwardResult afterAward(const Progress&, unsigned cup, unsigned engineClass, bool mirror, unsigned place);
// START skips a credits part only once that ending has been seen (fn_1_BAE10).
bool creditsSkippable(const UnlockWords&, Credits);
// Congratulations page (fn_1_BC334): sets bit 57 (normal) / 56 (true); true if it changed
// (the caller then marks the save dirty, retail markLicensesDirty).
bool markEndingSeen(UnlockWords&, Credits);

// Character select (page 0x6B). Buttons in retail order: three weight columns of
// rows*2 buttons each (light, medium, heavy), then the Mii buttons.
struct CharacterGrid {
    unsigned rows = 2;                   // layout CharacterSelect<rows>_<column>
    unsigned miiButtons = 0;             // 0..2
    std::array<int, 24> button{};        // [column * rows * 2 + k]: retail id, -1 = "?" (not selectable)
    std::array<int, 2> mii{-1, -1};      // retail Mii ids (medium size placeholders 0x1E / 0x20)
    unsigned count() const { return rows * 6 + miiButtons; }
    int character(unsigned slot) const;  // -1 for "?" slots and out of range
};
CharacterGrid characterGrid(const UnlockWords&);
// Button<k> of CharacterSelect<rows>_<column> at visual row r (0 top, 1 bottom) and
// visual column c (0..rows-1) inside its weight band: the cooked layout positions
// (menu_assets.hpp CharacterSelect2/3/4_0) put buttons 0,1 / 2,3 in the first two
// columns and 4+2(c-2)+r further right.
inline unsigned characterButton(unsigned c, unsigned r) { return c < 2 ? c + 2 * r : 4 + (c - 2) * 2 + r; }

// Vehicle select (page 0x6C). Per weight class: a kart column and a bike column,
// each the unlocked vehicles of that kind in retail order; rows is common to all.
struct VehicleColumns {
    unsigned rows = 3;                   // layout MachineSelect{1,2}_<rows>, 3..6
    std::array<std::array<int, 6>, 2> column{};   // [kind][row]: retail id, -1 = "?" slot
};
VehicleColumns vehicleColumns(const UnlockWords&, unsigned weight);

// Class page (page 0x6A): 3 buttons (ClassSelect) without Mirror, 4 (ClassSelect2) with it.
inline unsigned engineClassButtons(const UnlockWords& f) { return mirrorUnlocked(f) ? 4u : 3u; }
}
