#pragma once
// The retail save file (rksys.dat, RKSD0006) and its four licences.
// Layout, defaults and rules follow retail SaveManager (StaticR, PAL):
//   SaveManager_init 0x8054484C, fn_1_37D50 0x80548104 (new file),
//   RawLicense_reset 0x80548300, SaveManager_createLicense 0x80548030,
//   SaveManager_eraseLicense 0x80547FBC, UnlockFlags_get/set 0x8054994C/0x805498FC,
//   cup result fn_1_ACC9C 0x805BD050, statistics fn_1_273654 0x80783A08 and
//   fn_1_34C1EC 0x8085C5A0, Time Trial insertion License_getLeaderboardSlot
//   0x805490F4, course slot table 0x808911C0 (getSlotForCourseId 0x80552D90).
// The file holds the retail 0x28000-byte core (CRC-32 at 0x27FFC). Ghosts stay
// .rkg files on the SD card. Plan and differences: findings/r1_save_plan.md.
#include <array>
#include <cstddef>
#include <cstdint>
namespace mkw {
// Owner's switch: false = a new licence starts locked like retail (default).
// true = createLicense() sets every reward bit (characters, vehicles, cups,
// Mirror, kart/bike classes) but never the ending bits 0x38/0x39.
inline constexpr bool NewLicenseStartsUnlocked = false;

enum class Trophy : std::uint8_t { Gold = 0, Silver = 1, Bronze = 2, None = 3 };
using GpRank = std::uint8_t;          // retail 0 = 3 stars … 3 = A … 8 = F
struct CupRecord { bool played = false; Trophy trophy = Trophy::None; GpRank rank = 8; };
using UnlockWords = std::array<std::uint32_t, 4>;   // RKPD +0x30
using MiiName = std::array<char16_t, 10>;
using MiiId = std::array<std::uint8_t, 8>;
struct TimeRecord { bool valid = false; std::uint32_t ms = 0;
                    std::uint8_t character = 0, vehicle = 0, controller = 2; bool driverUnknown = false; };
// One finished race or battle (fn_1_273654 + fn_1_34C1EC inputs).
// character/vehicle/course/stage: retail IDs (~0u = none); course is the
// retail course id, stage the battle stage 0..9 (retail order); mode is the
// retail RaceConfig game mode (0 GP, 1 VS, 2 TT, 3 battle, ...); place 1..12
// and racers count the player's final place and the number of racers.
struct RaceTally { bool race = false, battle = false; unsigned character = ~0u, vehicle = ~0u,
                   course = ~0u, stage = ~0u; unsigned place = 0, racers = 0; unsigned mode = 0;
                   // fn_1_273368 (PAL 0x8078371C) at the finish, from the kart's stats block
                   // (KartSettings +0x38): distance x 1e-5 (+ in first place, + in GP/VS),
                   // item hits given/taken and tricks. Added fields, defaults keep old callers.
                   float distance = 0, distanceFirst = 0, distanceVs = 0;
                   unsigned hitsGiven = 0, hitsTaken = 0, tricks = 0; };
// The licence records pages (RecordsPage 0xCE-0xD2, fill functions PAL
// 0x8085F5EC, 0x8085F934, 0x8085FA70, 0x8085FC0C, 0x8085FD24), as retail
// computes them from the licence. favourite*: retail id, -1 when never used.
struct LicenseRecordsView {
    unsigned overallRank = 7;           // 0 = 3 stars ... 8 = F (BMG 0xD2D + rank)
    std::uint32_t distance = 0, races = 0, battles = 0, wheelPercent = 0, tricks = 0;
    int favouriteCharacter = -1, favouriteVehicle = -1, favouriteCourse = -1, favouriteStage = -1;
    std::uint32_t ghostsSent = 0, ghostsReceived = 0;
    unsigned vr = 5000, br = 5000;
    std::uint32_t wfcRaceWins = 0, wfcRaceLosses = 0, wfcBattleWins = 0, wfcBattleLosses = 0;
    std::uint32_t firstPlaces = 0, firstPercent = 0, hitsGiven = 0, hitsTaken = 0;
    std::uint32_t ghostWins = 0, ghostLosses = 0, tournaments = 0;
};
enum class SaveBoot { Ready, Created, Corrupt, Unwritable };

class SaveStore {
public:
    static constexpr unsigned Licenses = 4;
    // Boot (main.cpp only)
    SaveBoot boot(const char* folder);      // load, or create + migrate
    bool recreate();                        // corrupt page "OK"
    void disableSaving();                   // "Start without saving"
    bool savingEnabled() const;
    // Licences (licence pages)
    bool occupied(unsigned slot) const;
    bool miiMissing(unsigned slot) const;
    MiiName name(unsigned slot) const;
    bool createLicense(unsigned slot, const MiiName&, const MiiId&);   // + saveNow()
    bool changeMii(unsigned slot, const MiiName&, const MiiId&);       // + saveNow()
    bool eraseLicense(unsigned slot);                                  // + saveNow()
    bool select(unsigned slot);
    int current() const;                    // -1: none selected
    // GP end (GP-end agent). engineClass 0 50cc 1 100cc 2 150cc 3 Mirror;
    // cup 0 Mushroom … 7 Lightning (= courseChoice / 4); place 1..12.
    // Applies fn_1_ACC9C's keep-the-best rule; returns true if the record
    // changed (then it has called markDirty()).
    bool recordCupResult(unsigned engineClass, unsigned cup, unsigned place, GpRank rank);
    CupRecord cup(unsigned engineClass, unsigned cup) const;              // current licence
    CupRecord cup(unsigned slot, unsigned engineClass, unsigned cup) const; // licence cards
    // Unlock flags (unlocks/credits agent). Retail bit numbers (UnlockFlags_get).
    bool unlocked(unsigned bit) const;      // current licence
    void setUnlocked(unsigned bit);         // caller then calls saveNow() (Unlock page) or markDirty() (endings)
    UnlockWords unlockWords(unsigned slot) const;
    UnlockWords unlockUnion() const;        // SaveManager_getUnlockFlagsUnion
    bool anyLicenseUnlocked(unsigned bit) const;   // TitlePage_onInit, bit 0x38
    // Counters
    void tallyRace(const RaceTally&);       // marks dirty
    std::uint32_t raceCount() const;        // RKPD 0xB4 (unlock alternative route)
    std::uint32_t battleCount() const;      // RKPD 0xB8
    // Time Trials (course = retail/Kinoko course id; rank 0..4, 5 = best lap)
    TimeRecord timeRecord(unsigned course, unsigned rank = 0) const;
    int recordTime(unsigned course, const TimeRecord&);   // rank written, -1 none; marks dirty
    unsigned timeTrialCourses() const;      // courses with a rank-1 time (unlock rule)
    // Drift choice (RKPD 0xEA)
    bool automaticDrift() const;
    void setAutomaticDrift(bool);           // marks dirty
    // Writes
    void markDirty();        // markLicensesDirty: written at the next section change
    bool saveNow();          // requestSaveLicenses: written now
    bool flushIfDirty();     // main.cpp: every section change, race leave, app exit

    // ---- Additions beyond the section 6 sketch (no sketch signature changed) ----
    // The best lap of a run (rank 5 of the Time Trial table, retail RKPD 0x49C0):
    // written when faster than the stored one. Returns true if written.
    bool recordBestLap(unsigned course, const TimeRecord&);
    LicenseRecordsView records(unsigned slot) const;
    // The last write failed (retail error 7 → the cannot-save page, section 0x13).
    bool writeFailed() const { return writeFailed_; }
    // Raw core for host tests and diagnostics (0x28000 bytes).
    static constexpr unsigned CoreSize = 0x28000;
    const std::uint8_t* core() const { return core_.data(); }
    bool dirty() const { return dirty_; }
private:
    std::uint8_t* license(unsigned slot);
    const std::uint8_t* license(unsigned slot) const;
    bool write();
    void migrate();
    std::array<std::uint8_t, CoreSize> core_{};
    char folder_[96] = {};
    std::uint32_t tailSize_ = 0;            // bytes after the core in an imported full-size file
    int current_ = -1;
    bool loaded_ = false, saving_ = true, dirty_ = false, writeFailed_ = false;
};
SaveStore& saveStore();
// best-times.bin (v0.7.0 and older): one little-endian u32 per course in the
// port catalog order (course_catalog.hpp Courses[0..31], static_assert in
// main.cpp); these are their retail course ids.
inline constexpr std::uint8_t LegacyBestTimesCourses[32] = {
    0x08, 0x01, 0x02, 0x04, 0x00, 0x05, 0x06, 0x07, 0x09, 0x0F, 0x0B, 0x03, 0x0E, 0x0A, 0x0C, 0x0D,
    0x10, 0x14, 0x19, 0x1A, 0x1B, 0x1F, 0x17, 0x12, 0x15, 0x1E, 0x1D, 0x11, 0x18, 0x16, 0x13, 0x1C};
// Retail save slot of a retail course id (table PAL 0x808911C0), -1 if none.
int saveSlotForCourse(unsigned course);
// NETCalcCRC32 as SaveManager uses it over bytes 0..0x27FFB (standard CRC-32).
std::uint32_t saveCrc32(const std::uint8_t* data, std::size_t size);
}
