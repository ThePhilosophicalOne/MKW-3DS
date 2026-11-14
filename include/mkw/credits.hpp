#pragma once
// Retail staff credits and ending (StaticR, PAL; plan: findings/r1_unlocks_credits_plan.md 1.6).
//   part 1  sections 0x39 (normal) / 0x3A (true): ending_demo (course 0x3A = 58) run in the
//           race scene, game mode 12, music STRM_O_ENDING_PART_A; page 0x3D with the
//           "EndingDemoStaffRoll" text, 6290 frames (fn_1_BAE10 0x805CB1C4, fn_1_BB3A8 0x805CB75C)
//   part 2  sections 0x3B / 0x3C: page 0x3D "EndingMovieStaffRoll", 8060 frames, and page 0x3E
//           playing thp/ending/ending_normal.thp / ending_true.thp in the EndingMovie panes
//           MovieTopLeft / MovieBottomRight (fn_1_BBCF4 0x805CC0A8, fn_1_BBE08 0x805CC1BC)
//   end     sections 0x3D / 0x3E, page 0x3F Congratulations (EndingCloseBG NormalEndingBG /
//           TrueEndingBG; fn_1_BC148 0x805CC4FC, fn_1_BC334 0x805CC6E8, fn_1_BC850 0x805CCC04)
//   skip    START (input handler 2, fn_1_BB73C) is installed only when that ending was seen
//           before (bit 57 normal / 56 true); it jumps to the Congratulations section.
// Pure logic: the caller draws and plays; this decides what and when.
#include <mkw/unlock_tables.hpp>
#include <mkw/unlocks.hpp>
#include <array>
#include <cstdint>

namespace mkw::credits {
enum class Part : std::uint8_t { Demo, Movie };
inline constexpr unsigned DemoFrames = 0x1892;    // 6290
inline constexpr unsigned MovieFrames = 0x1F7C;   // 8060
inline constexpr unsigned EndingCourse = 0x3A;    // ending_demo, Kinoko course 58
inline constexpr unsigned CreditsGameMode = 12;   // RaceConfig game mode (isAwardsOrCredits 0x80739888: 11, 12)
// Section BGM (Section_GetSoundId fn_1_1218B4 0x80631C68): 0x39/0x3A play
// STRM_O_ENDING_PART_A (206); 0x3B..0x3E none (part 2 uses the movie's own audio).
inline constexpr unsigned DemoMusic = 206;
// MenuOther_U StaffRoll.bmg templates: title pane / name pane / name pane without title.
inline constexpr std::uint16_t TitleTemplate = 0x22C4, NameTemplate = 0x22C5, NameOnlyTemplate = 0x22C6;

// One of the three staff-roll positions (StaffRoll{Center,Top,Bottom}{Title,Name}).
struct Slot {
    enum class Kind : std::uint8_t { Hidden, TitleAndName, NameOnly } kind = Kind::Hidden;
    std::uint16_t title = 0, name = 0;   // message ids passed to the templates
};
// Animation group 0 of the six text controls: retail Group_setAnimation(1) = in, (3) = out.
enum class TextAnimation : std::uint8_t { None, In, Out };

class StaffRoll {
public:
    explicit StaffRoll(Part part);
    // One page frame. elapsed = page frames minus the page's first-frame offset
    // (retail +0xA66: 0 or 1, see firstFrameOffset()). Returns the animation the six
    // controls start this frame; slots() holds the texts shown since the last In.
    TextAnimation step(unsigned elapsed);
    const std::array<Slot, 3>& slots() const { return slots_; }   // center, top, bottom
    unsigned entry() const { return index_; }
    bool showing() const { return !armed_; }
    unsigned duration() const { return part_ == Part::Demo ? DemoFrames : MovieFrames; }
    Part part() const { return part_; }
private:
    Part part_;
    const unlock_tables::StaffRollEntry* table_;
    unsigned count_, index_ = 0;
    bool armed_ = true;                 // retail +0xA64
    std::array<Slot, 3> slots_{};
};
// fn_1_BB350 / fn_1_BBF38: the 0/1 offset the staff roll subtracts from the page frame.
// Part 1: page frame at activation != 0. Part 2: re-taken when the movie starts.
inline unsigned firstFrameOffset(unsigned pageFrameAtStart) { return pageFrameAtStart != 0; }

// The movie pane in view, by frames since the movie started (fn_1_BBE08):
// 0 = MovieTopLeft, 1 = MovieBottomRight, -1 = both out.
int moviePane(unsigned sinceMovieStart);

enum class Kind : std::uint8_t { Normal, True };
inline unlock::Credits creditsOf(Kind k) { return k == Kind::True ? unlock::Credits::True : unlock::Credits::Normal; }
// Retail sections, for logs and the section-edge table.
inline unsigned demoSection(Kind k) { return k == Kind::True ? 0x3A : 0x39; }
inline unsigned movieSection(Kind k) { return k == Kind::True ? 0x3C : 0x3B; }
inline unsigned congratulationsSection(Kind k) { return k == Kind::True ? 0x3E : 0x3D; }
// thp/ending movie; the _50 files are PAL 50 Hz only (isPal50) and not used.
inline const char* movieSource(Kind k) { return k == Kind::True ? "thp/ending/ending_true.thp" : "thp/ending/ending_normal.thp"; }
inline const char* movieStream(Kind k) { return k == Kind::True ? "movies/ending_true.gmv" : "movies/ending_normal.gmv"; }
inline const char* movieAudio(Kind k) { return k == Kind::True ? "movies/ending_true.wav" : "movies/ending_normal.wav"; }

enum class Stage : std::uint8_t { Demo, Movie, Congratulations, Done };
// Where the game goes after the Congratulations page.
enum class Exit : std::uint8_t { UnlockPages, MainMenu, Title };

class Flow {
public:
    // skippable: unlock::creditsSkippable(flags, creditsOf(kind)), read when the credits start.
    Flow(Kind kind, bool skippable);
    Kind kind() const { return kind_; }
    Stage stage() const { return stage_; }
    // A 60 Hz frame of the current stage (page frames counted from the stage start).
    // Returns true when the stage changed this frame (the caller changes scene).
    bool tick();
    // START during part 1 or 2: accepted only when skippable; the section changes on the next tick.
    bool start();
    // A on the Congratulations page (handler 0, fn_1_BCDF8). The Wii Message Board photo
    // steps (0x1E17..0x1E1B, 0x20D5/0x20D6) are N/A-3DS, so A leaves at once.
    bool accept();
    // The port only: part 1's stage could not load, go on with part 2.
    void endDemo() { if (stage_ == Stage::Demo) enter(Stage::Movie); }
    // Part 2: the movie's first frame is on screen (retail fn_1_BBF38, the movie
    // page's start callback): the pane timeline starts here and the staff roll
    // re-takes its 0/1 frame offset.
    void movieStarted();
    // The movie pane in view now (moviePane), -1 before the movie started.
    int pane() const { return stage_ == Stage::Movie && movieStart_ >= 0 ? moviePane(frame_ - unsigned(movieStart_)) : -1; }
    // The staff-roll animation the six text controls start this frame (StaffRoll::step).
    TextAnimation animation() const { return animation_; }
    // Frames since the current stage started.
    unsigned frame() const { return frame_; }
    StaffRoll& roll() { return roll_; }
    // fn_1_BC850: normal ending -> pending unlock pages or the menus (section 0x41);
    // true ending -> the title (section 0x3F).
    Exit exit(bool unlockPending) const;
private:
    void enter(Stage);
    Kind kind_;
    bool skippable_, skip_ = false, accepted_ = false;
    Stage stage_ = Stage::Demo;
    unsigned frame_ = 0, offset_ = 0;
    int movieStart_ = -1;
    TextAnimation animation_ = TextAnimation::None;
    StaffRoll roll_{Part::Demo};
};

// Racedata_initCredits 0x80531070: the ending_demo field is the award scenario's
// racers, every one CPU-driven: first the local humans in slot order, then the
// others by their scenario finish position 1..12. Input: per award-scenario slot,
// whether it is a local human, whether it holds a racer, and its finish position.
struct AwardRacer { bool present = false, local = false; unsigned finishPosition = 0; };
// Returns award-scenario slot indices in ending_demo start order (KTPT 0..n-1); -1 pads.
std::array<int, 12> endingOrder(const std::array<AwardRacer, 12>&);
}
