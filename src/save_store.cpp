#include <mkw/save_store.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

// Retail references are in include/mkw/save_store.hpp and r1_save_plan.md.
// Every field is read and written in place in the big-endian core, by byte
// offset, as the retail file stores it (no parsed copy).
namespace mkw {
namespace {
constexpr unsigned FullSize = 0x2BC000, LicenseSize = 0x8CC0, GlobalOffset = 0x23308, CrcOffset = 0x27FFC;
constexpr std::uint32_t CounterLimit = 99999; // fn_1_273654 / fn_1_34C1EC clamp
// RKPD offsets.
constexpr unsigned Name = 0x14, Mii = 0x28, Flags = 0x30, Drift = 0xEA, Races = 0xB4, Battles = 0xB8,
                   FirstPlaces = 0xDC, OfflineWins = 0x88, OfflineLosses = 0x8C, WfcWins = 0x98, WfcLosses = 0x9C,
                   Characters = 0xEC, Vehicles = 0x11E, CourseCounts = 0x166, StageCounts = 0x1A6,
                   Cups = 0x1C0, Times = 0xDC0;
// Port marker in a Time Trial entry's reserved tail (0x52-0x5F, zero in retail):
// the time came from best-times.bin without a ghost naming its driver.
constexpr unsigned UnknownDriverByte = 0x5F;
// getSlotForCourseId (PAL 0x80552D90): the table at PAL 0x808911C0 lists the
// retail course id of every save slot.
constexpr std::uint8_t SlotCourse[32] = {0x08, 0x01, 0x02, 0x04, 0x00, 0x05, 0x06, 0x07, 0x09, 0x0F, 0x03,
                                         0x0B, 0x0A, 0x0E, 0x0C, 0x0D, 0x1F, 0x19, 0x18, 0x1E, 0x1B, 0x1A,
                                         0x1D, 0x1C, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
constexpr const std::uint8_t* LegacyCourse = LegacyBestTimesCourses;

unsigned be16(const std::uint8_t* p) { return unsigned(p[0]) << 8 | p[1]; }
std::uint32_t be32(const std::uint8_t* p)
{
    return std::uint32_t(p[0]) << 24 | std::uint32_t(p[1]) << 16 | std::uint32_t(p[2]) << 8 | p[3];
}
void put16(std::uint8_t* p, unsigned v) { p[0] = std::uint8_t(v >> 8); p[1] = std::uint8_t(v); }
void put32(std::uint8_t* p, std::uint32_t v)
{
    p[0] = std::uint8_t(v >> 24); p[1] = std::uint8_t(v >> 16); p[2] = std::uint8_t(v >> 8); p[3] = std::uint8_t(v);
}
// NETCalcCRC32: the standard reflected CRC-32.
constexpr auto CrcTable = [] {
    std::array<std::uint32_t, 256> t{};
    for (unsigned i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (unsigned k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
        t[i] = c;
    }
    return t;
}();
std::uint32_t crc32(const std::uint8_t* p, std::size_t n)
{
    std::uint32_t c = ~0u;
    for (std::size_t i = 0; i < n; ++i) c = (c >> 8) ^ CrcTable[(c ^ p[i]) & 0xFF];
    return ~c;
}
// RawLicense_reset (PAL 0x80548300).
void resetLicense(std::uint8_t* p)
{
    std::memset(p, 0, LicenseSize);
    put16(p + 0x80, 0x5008); put16(p + 0x82, 0x1000); put16(p + 0x84, 0x5008); put16(p + 0x86, 0x1000);
    put16(p + 0xB0, 5000); put16(p + 0xB2, 5000);
    p[Drift] = std::uint8_t((p[Drift] & 0x3F) | 0x80);
}
// fn_1_37D50 (PAL 0x80548104) / SaveManager_resetAsync: a new file.
void initialize(std::uint8_t* b)
{
    std::memset(b, 0, SaveStore::CoreSize);
    for (unsigned s = 0; s < SaveStore::Licenses; ++s) resetLicense(b + 8 + s * LicenseSize);
    std::uint8_t* g = b + GlobalOffset;
    for (unsigned i = 0; i < 10; ++i) put32(g + 8 + i * 0x384, 0xFFFFFFFFu);
    put32(g + 4, 0x27);
    // Region nibble: SystemManager +0x84; the port runs the USA disc (1, Americas).
    put16(b + 0x26B0A, (be16(b + 0x26B0A) & 0x0FFF) | (1u << 12));
    std::memcpy(g, "RKGD", 4);
    std::memcpy(b, "RKSD0006", 8);
    put32(b + CrcOffset, crc32(b, CrcOffset));
}
bool valid(const std::uint8_t* b)
{
    return !std::memcmp(b, "RKSD0006", 8) && be32(b + CrcOffset) == crc32(b, CrcOffset);
}
bool exists(const char* p)
{
    std::FILE* f = std::fopen(p, "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}
long fileSize(const char* p)
{
    std::FILE* f = std::fopen(p, "rb");
    if (!f) return -1;
    long n = std::fseek(f, 0, SEEK_END) ? -1 : std::ftell(f);
    std::fclose(f);
    return n;
}
// Reads the core of a retail-sized or core-sized file and checks it.
bool readCore(const char* p, std::uint8_t* b, std::uint32_t& tail)
{
    const long n = fileSize(p);
    if (n != long(SaveStore::CoreSize) && n != long(FullSize)) return false;
    std::FILE* f = std::fopen(p, "rb");
    if (!f) return false;
    const bool ok = std::fread(b, 1, SaveStore::CoreSize, f) == SaveStore::CoreSize;
    std::fclose(f);
    if (!ok || !valid(b)) return false;
    tail = std::uint32_t(n) - SaveStore::CoreSize;
    return true;
}
void path(char* out, std::size_t n, const char* folder, const char* name)
{
    std::snprintf(out, n, "%s/%s", folder, name);
}
void makeFolders(const char* folder)
{
    char p[128];
    std::snprintf(p, sizeof p, "%s", folder);
    for (char* c = p + 1; *c; ++c)
        if (*c == '/' && c[-1] != ':') { *c = 0; mkdir(p, 0777); *c = '/'; }
    mkdir(p, 0777);
}
std::uint32_t packTime(std::uint32_t ms)
{
    return (ms / 60000) << 25 | ((ms / 1000) % 60) << 18 | (ms % 1000) << 8;
}
std::uint32_t timeMs(std::uint32_t packed)
{
    const unsigned m = packed >> 25, s = (packed >> 18) & 0x7F, ms = (packed >> 8) & 0x3FF;
    return s < 60 && ms < 1000 ? (m * 60 + s) * 1000 + ms : 0;
}
// Every reward bit (characters 0-13, cups 14-29, kart/bike classes 30-31,
// vehicles 34-51, Mirror 52). Never the ending bits 0x38/0x39.
void unlockEverything(std::uint8_t* p)
{
    UnlockWords w{};
    for (unsigned bit = 0; bit <= 52; ++bit)
        if (bit != 32 && bit != 33) w[bit / 32] |= std::uint32_t(1) << (bit % 32);
    for (unsigned i = 0; i < 4; ++i) put32(p + Flags + 4 * i, w[i]);
}
void increment(std::uint8_t* p)
{
    const std::uint32_t v = be32(p);
    if (v < CounterLimit) put32(p, v + 1);
}
void add(std::uint8_t* p, std::uint32_t n)
{
    const std::uint32_t v = be32(p);
    if (v < CounterLimit) put32(p, std::min<std::uint32_t>(v + n, CounterLimit));
}
void increment16(std::uint8_t* p)
{
    const unsigned v = be16(p);
    if (v != 0xFFFF) put16(p, v + 1);
}
// Time Trial entry (RawSave_readLeaderboardEntry, PAL 0x8054594C).
TimeRecord readEntry(const std::uint8_t* e)
{
    TimeRecord r;
    if (!(e[0x50] & 0x80)) return r;
    const std::uint32_t w = be32(e + 0x4C);
    r.valid = true;
    r.ms = timeMs(w);
    r.vehicle = std::uint8_t((w >> 2) & 0x3F);
    r.character = std::uint8_t(e[0x50] & 0x7F);
    r.controller = std::uint8_t(e[0x51] >> 5);
    r.driverUnknown = e[UnknownDriverByte] != 0;
    return r;
}
void writeEntry(std::uint8_t* e, const TimeRecord& r)
{
    // The 0x4A Mii block and its CRC-16 stay zero: the 3DS has no Wii Mii data.
    std::memset(e, 0, 0x60);
    put32(e + 0x4C, packTime(r.ms) | std::uint32_t(r.vehicle & 0x3F) << 2);
    e[0x50] = std::uint8_t(0x80 | (r.character & 0x7F));
    e[0x51] = std::uint8_t((r.controller & 7) << 5);
    if (r.driverUnknown) e[UnknownDriverByte] = 1;
}
// Retail compares minutes, then seconds, then milliseconds; equal is not better.
bool faster(std::uint32_t a, std::uint32_t b) { return a < b; }
}

std::uint32_t saveCrc32(const std::uint8_t* data, std::size_t size) { return crc32(data, size); }

int saveSlotForCourse(unsigned course)
{
    for (unsigned i = 0; i < 32; ++i)
        if (SlotCourse[i] == course) return int(i);
    return -1;
}

std::uint8_t* SaveStore::license(unsigned slot) { return core_.data() + 8 + slot * LicenseSize; }
const std::uint8_t* SaveStore::license(unsigned slot) const { return core_.data() + 8 + slot * LicenseSize; }

SaveBoot SaveStore::boot(const char* folder)
{
    std::snprintf(folder_, sizeof folder_, "%s", folder);
    current_ = -1; dirty_ = false; saving_ = true; writeFailed_ = false; tailSize_ = 0;
    char main[128], backup[128];
    path(main, sizeof main, folder_, "rksys.dat");
    path(backup, sizeof backup, folder_, "rksys.bak");
    if (readCore(main, core_.data(), tailSize_) || readCore(backup, core_.data(), tailSize_)) {
        loaded_ = true;
        return SaveBoot::Ready;
    }
    if (exists(main) || exists(backup)) {
        // Section 0x11: the page offers a new file (recreate()).
        initialize(core_.data());
        loaded_ = false;
        return SaveBoot::Corrupt;
    }
    // Section 0x10: no file yet, created at once ("Saving..."), with the
    // v0.7.0 best times and ghosts carried into licence 1.
    initialize(core_.data());
    loaded_ = true;
    migrate();
    if (!write()) return SaveBoot::Unwritable;
    return SaveBoot::Created;
}

bool SaveStore::recreate()
{
    char main[128], backup[128], aside[128];
    path(main, sizeof main, folder_, "rksys.dat");
    path(backup, sizeof backup, folder_, "rksys.bak");
    path(aside, sizeof aside, folder_, "rksys-corrupt.dat");
    // Retail deletes the damaged file; the port keeps it aside.
    const char* bad = exists(main) ? main : exists(backup) ? backup : nullptr;
    if (bad) {
        std::remove(aside);
        if (std::rename(bad, aside)) std::remove(bad);
    }
    if (bad == main) std::remove(backup);
    initialize(core_.data());
    tailSize_ = 0; current_ = -1; loaded_ = true; saving_ = true;
    return write();
}

void SaveStore::disableSaving() { saving_ = false; dirty_ = false; }
bool SaveStore::savingEnabled() const { return saving_; }

void SaveStore::migrate()
{
    char legacy[128];
    path(legacy, sizeof legacy, folder_, "best-times.bin");
    std::array<std::uint32_t, 32> times{};
    bool any = false;
    if (fileSize(legacy) == 128)
        if (std::FILE* f = std::fopen(legacy, "rb")) {
            std::uint8_t b[128];
            if (std::fread(b, 1, sizeof b, f) == sizeof b)
                for (unsigned i = 0; i < 32; ++i) {
                    times[i] = std::uint32_t(b[4 * i]) | std::uint32_t(b[4 * i + 1]) << 8 |
                               std::uint32_t(b[4 * i + 2]) << 16 | std::uint32_t(b[4 * i + 3]) << 24;
                    any |= times[i] != 0;
                }
            std::fclose(f);
        }
    struct Ghost { bool found = false; std::uint32_t ms = 0; std::uint8_t character = 0, vehicle = 0, controller = 2; };
    std::array<Ghost, 32> ghosts{};
    for (unsigned i = 0; i < 32; ++i) {
        char name[160];
        std::snprintf(name, sizeof name, "%s/ghosts/pb-%02x.rkg", folder_, unsigned(LegacyCourse[i]));
        std::FILE* f = std::fopen(name, "rb");
        if (!f) continue;
        std::uint8_t h[0x10];
        if (std::fread(h, 1, sizeof h, f) == sizeof h && !std::memcmp(h, "RKGD", 4)) {
            const std::uint32_t a = be32(h + 4), b = be32(h + 8);
            auto& g = ghosts[i];
            g.found = true;
            g.ms = (a >> 25) * 60000u + ((a >> 18) & 0x7F) * 1000u + ((a >> 8) & 0x3FF);
            g.vehicle = std::uint8_t(b >> 26);
            g.character = std::uint8_t((b >> 20) & 0x3F);
            g.controller = std::uint8_t(b & 0xF);
            any = true;
        }
        std::fclose(f);
    }
    if (!any) return;
    // Licence 1 with no Mii: retail's "Mii deleted" state, so the first press
    // of its card asks for a Mii (fn_1_DB3EC, PAL 0x805EB7A0).
    std::uint8_t* p = license(0);
    resetLicense(p);
    if (NewLicenseStartsUnlocked) unlockEverything(p);
    std::memcpy(p, "RKPD", 4);
    for (unsigned i = 0; i < 32; ++i) {
        const std::uint32_t ms = times[i] ? times[i] : ghosts[i].found ? ghosts[i].ms : 0;
        if (!ms || ms >= 128u * 60000u) continue;
        TimeRecord r;
        r.valid = true; r.ms = ms;
        if (ghosts[i].found && ghosts[i].ms == ms) {
            r.character = ghosts[i].character; r.vehicle = ghosts[i].vehicle; r.controller = ghosts[i].controller;
        } else
            r.driverUnknown = true;
        const int slot = saveSlotForCourse(LegacyCourse[i]);
        writeEntry(p + Times + unsigned(slot) * 0x60, r);
    }
}

bool SaveStore::occupied(unsigned slot) const { return slot < Licenses && !std::memcmp(license(slot), "RKPD", 4); }
bool SaveStore::miiMissing(unsigned slot) const
{
    if (!occupied(slot)) return false;
    const std::uint8_t* p = license(slot) + Mii;
    return std::all_of(p, p + 8, [](std::uint8_t c) { return c == 0; });
}
MiiName SaveStore::name(unsigned slot) const
{
    MiiName n{};
    if (occupied(slot))
        for (unsigned i = 0; i < n.size(); ++i) n[i] = char16_t(be16(license(slot) + Name + 2 * i));
    return n;
}
bool SaveStore::createLicense(unsigned slot, const MiiName& n, const MiiId& id)
{
    if (slot >= Licenses) return false;
    std::uint8_t* p = license(slot);
    resetLicense(p);
    if (NewLicenseStartsUnlocked) unlockEverything(p);
    std::memcpy(p + Mii, id.data(), 8);
    for (unsigned i = 0; i < n.size(); ++i) put16(p + Name + 2 * i, n[i]);
    std::memcpy(p, "RKPD", 4);
    saveNow();
    return true;
}
bool SaveStore::changeMii(unsigned slot, const MiiName& n, const MiiId& id)
{
    if (!occupied(slot)) return false;
    std::uint8_t* p = license(slot);
    std::memcpy(p + Mii, id.data(), 8);
    for (unsigned i = 0; i < n.size(); ++i) put16(p + Name + 2 * i, n[i]);
    saveNow();
    return true;
}
bool SaveStore::eraseLicense(unsigned slot)
{
    // Owner decision: the licence's .rkg ghost files stay on the SD card.
    if (slot >= Licenses) return false;
    resetLicense(license(slot));
    if (current_ == int(slot)) current_ = -1;
    saveNow();
    return true;
}
bool SaveStore::select(unsigned slot)
{
    if (!occupied(slot)) return false;
    current_ = int(slot);
    return true;
}
int SaveStore::current() const { return current_; }

bool SaveStore::recordCupResult(unsigned engineClass, unsigned cup, unsigned place, GpRank rank)
{
    if (current_ < 0 || engineClass >= 4 || cup >= 8 || place < 1 || place > 12 || rank > 8) return false;
    std::uint8_t* c = license(unsigned(current_)) + Cups + engineClass * 0x300 + cup * 0x60;
    unsigned trophy = place <= 3 ? place - 1 : 3;
    unsigned grade = rank;
    if (c[0x52] & 0x80) {
        const unsigned oldTrophy = (be32(c + 0x4C) >> 6) & 3, oldRank = be16(c + 0x50) & 0xF;
        const bool better = oldTrophy > trophy || grade < oldRank;
        if (!better) return false;
        trophy = std::min(trophy, oldTrophy);
        grade = std::min(grade, oldRank);
    }
    // fn_1_ACC9C writes the time as zero; its Mii, character and vehicle come
    // from uninitialised stack bytes there and are never shown: zero here.
    std::memset(c, 0, 0x60);
    put32(c + 0x4C, trophy << 6);
    put16(c + 0x50, grade);
    c[0x52] = 0x80;
    markDirty();
    return true;
}
CupRecord SaveStore::cup(unsigned engineClass, unsigned cupIndex) const
{
    return current_ < 0 ? CupRecord{} : cup(unsigned(current_), engineClass, cupIndex);
}
CupRecord SaveStore::cup(unsigned slot, unsigned engineClass, unsigned cupIndex) const
{
    CupRecord r;
    if (!occupied(slot) || engineClass >= 4 || cupIndex >= 8) return r;
    const std::uint8_t* c = license(slot) + Cups + engineClass * 0x300 + cupIndex * 0x60;
    if (!(c[0x52] & 0x80)) return r;
    r.played = true;
    r.trophy = Trophy((be32(c + 0x4C) >> 6) & 3);
    r.rank = GpRank(be16(c + 0x50) & 0xF);
    return r;
}

bool SaveStore::unlocked(unsigned bit) const
{
    if (current_ < 0 || bit >= 128) return false;
    return be32(license(unsigned(current_)) + Flags + 4 * (bit / 32)) & (std::uint32_t(1) << (bit % 32));
}
void SaveStore::setUnlocked(unsigned bit)
{
    if (current_ < 0 || bit >= 128) return;
    std::uint8_t* w = license(unsigned(current_)) + Flags + 4 * (bit / 32);
    put32(w, be32(w) | std::uint32_t(1) << (bit % 32));
}
UnlockWords SaveStore::unlockWords(unsigned slot) const
{
    UnlockWords w{};
    if (occupied(slot))
        for (unsigned i = 0; i < 4; ++i) w[i] = be32(license(slot) + Flags + 4 * i);
    return w;
}
UnlockWords SaveStore::unlockUnion() const
{
    UnlockWords u{};
    for (unsigned s = 0; s < Licenses; ++s) {
        const auto w = unlockWords(s);
        for (unsigned i = 0; i < 4; ++i) u[i] |= w[i];
    }
    return u;
}
bool SaveStore::anyLicenseUnlocked(unsigned bit) const
{
    return bit < 128 && (unlockUnion()[bit / 32] & (std::uint32_t(1) << (bit % 32)));
}

void SaveStore::tallyRace(const RaceTally& t)
{
    if (current_ < 0) return;
    std::uint8_t* p = license(unsigned(current_));
    // fn_1_273654 (PAL 0x80783A08), RacePage_afterCalc 0x78 frames after the finish.
    if (t.race) increment(p + Races);
    if (t.battle) increment(p + Battles);
    if (t.character != ~0u) increment16(p + Characters + 2 * std::min(t.character, 24u));
    if (t.vehicle < 36) increment16(p + Vehicles + 2 * t.vehicle);
    if (t.course < 32) increment16(p + CourseCounts + 2 * t.course);
    if (t.stage < 10) increment16(p + StageCounts + 2 * t.stage);
    // Distances (float, capped at 1e9) and the counters of the stats block.
    auto addFloat = [&](unsigned offset, float value) {
        float v; std::uint32_t bits = be32(p + offset); std::memcpy(&v, &bits, 4);
        if (v >= 1e9f) return;
        v = std::min(v + value, 1e9f); std::memcpy(&bits, &v, 4); put32(p + offset, bits);
    };
    addFloat(0xC4, t.distance);
    addFloat(0xE4, t.distanceVs);
    addFloat(0xE0, t.distanceFirst);
    add(p + 0xD0, t.hitsGiven);
    add(p + 0xD4, t.hitsTaken);
    add(p + 0xD8, t.tricks);
    // fn_1_34C1EC (PAL 0x8085C5A0): GP and VS (modes 0-1; 7-8 online).
    const bool offline = t.mode <= 1, online = t.mode == 7 || t.mode == 8;
    if ((offline || online) && t.place >= 1 && t.place <= t.racers) {
        if (t.place == 1) increment(p + FirstPlaces);
        add(p + (online ? WfcWins : OfflineWins), t.racers - t.place);
        add(p + (online ? WfcLosses : OfflineLosses), t.place - 1);
    }
    markDirty();
}
std::uint32_t SaveStore::raceCount() const { return current_ < 0 ? 0 : be32(license(unsigned(current_)) + Races); }
std::uint32_t SaveStore::battleCount() const { return current_ < 0 ? 0 : be32(license(unsigned(current_)) + Battles); }

TimeRecord SaveStore::timeRecord(unsigned course, unsigned rank) const
{
    const int slot = saveSlotForCourse(course);
    if (current_ < 0 || slot < 0 || rank > 5) return {};
    return readEntry(license(unsigned(current_)) + Times + rank * 0xC00 + unsigned(slot) * 0x60);
}
int SaveStore::recordTime(unsigned course, const TimeRecord& r)
{
    const int slot = saveSlotForCourse(course);
    if (current_ < 0 || slot < 0 || !r.ms || r.ms >= 128u * 60000u) return -1;
    std::uint8_t* base = license(unsigned(current_)) + Times + unsigned(slot) * 0x60;
    // License_getLeaderboardSlot (PAL 0x805490F4): the first of the five
    // ranks that is empty or slower; then the entries below move down one.
    int rank = -1;
    for (unsigned i = 0; i < 5 && rank < 0; ++i) {
        const TimeRecord old = readEntry(base + i * 0xC00);
        if (!old.valid || faster(r.ms, old.ms)) rank = int(i);
    }
    if (rank < 0) return -1;
    for (int i = 4; i > rank; --i) std::memcpy(base + i * 0xC00, base + (i - 1) * 0xC00, 0x60);
    TimeRecord w = r;
    w.valid = true;
    writeEntry(base + unsigned(rank) * 0xC00, w);
    markDirty();
    return rank;
}
bool SaveStore::recordBestLap(unsigned course, const TimeRecord& r)
{
    const int slot = saveSlotForCourse(course);
    if (current_ < 0 || slot < 0 || !r.ms || r.ms >= 128u * 60000u) return false;
    std::uint8_t* e = license(unsigned(current_)) + Times + 5 * 0xC00 + unsigned(slot) * 0x60;
    const TimeRecord old = readEntry(e);
    if (old.valid && !faster(r.ms, old.ms)) return false;
    TimeRecord w = r;
    w.valid = true;
    writeEntry(e, w);
    markDirty();
    return true;
}
unsigned SaveStore::timeTrialCourses() const
{
    unsigned n = 0;
    if (current_ >= 0)
        for (unsigned s = 0; s < 32; ++s) n += readEntry(license(unsigned(current_)) + Times + s * 0x60).valid;
    return n;
}

LicenseRecordsView SaveStore::records(unsigned slot) const
{
    LicenseRecordsView r;
    if (!occupied(slot)) return r;
    const std::uint8_t* p = license(slot);
    auto u32 = [&](unsigned o) { return std::min<std::uint32_t>(be32(p + o), CounterLimit); };
    auto f32 = [&](unsigned o) { float v; std::uint32_t b = be32(p + o); std::memcpy(&v, &b, 4); return v; };
    // Overall Ranking (fn_1_34F238 row 0): 8 - the mean of (8 - rank) over the
    // played cups; at 1 star or better, the worst rank of all 32 cups (7 for an
    // unplayed one), at most A.
    int sum = 0, played = 0;
    for (unsigned cc = 0; cc < 4; ++cc)
        for (unsigned c = 0; c < 8; ++c) {
            const std::uint8_t* e = p + Cups + cc * 0x300 + c * 0x60;
            if (e[0x52] & 0x80) { ++played; sum += 8 - int(be16(e + 0x50) & 0xF); }
        }
    int rank = played ? 8 - sum / played : 7;
    if (rank < 3) {
        int worst = 0;
        for (unsigned cc = 0; cc < 4; ++cc)
            for (unsigned c = 0; c < 8; ++c) {
                const std::uint8_t* e = p + Cups + cc * 0x300 + c * 0x60;
                if (!(e[0x52] & 0x80)) worst = 7;
                else if (worst < int(be16(e + 0x50) & 0xF)) worst = int(be16(e + 0x50) & 0xF);
            }
        rank = worst < 4 ? worst : 3;
    }
    r.overallRank = unsigned(rank);
    r.distance = std::uint32_t(std::min(f32(0xC4), 1e9f));
    r.races = u32(Races); r.battles = u32(Battles); r.tricks = u32(0xD8);
    // Wii Wheel % (fn_1_3936C): 100 x wheel races and battles / races and battles.
    const std::uint32_t all = r.races + r.battles, wheel = u32(0xBC) + u32(0xC0);
    r.wheelPercent = all ? std::min<std::uint32_t>(std::uint32_t(100.f * float(wheel) / float(all)), 100) : 0;
    // Favourites (fn_1_38FFC/390BC/3919C/392BC): the first highest counter.
    auto favourite = [&](unsigned offset, unsigned count, int base) {
        int best = -1; unsigned most = 0;
        for (unsigned i = 0; i < count; ++i)
            if (be16(p + offset + 2 * i) > most) { most = be16(p + offset + 2 * i); best = base + int(i); }
        return best;
    };
    r.favouriteCharacter = favourite(Characters, 25, 0);
    r.favouriteVehicle = favourite(Vehicles, 36, 0);
    r.favouriteCourse = favourite(CourseCounts, 32, 0);
    r.favouriteStage = favourite(StageCounts, 10, 0x20);
    r.ghostsSent = u32(0xC8); r.ghostsReceived = u32(0xCC);
    r.vr = be16(p + 0xB0); r.br = be16(p + 0xB2);
    r.wfcRaceWins = be32(p + 0x98); r.wfcRaceLosses = be32(p + 0x9C);
    r.wfcBattleWins = be32(p + 0xA0); r.wfcBattleLosses = be32(p + 0xA4);
    r.firstPlaces = u32(FirstPlaces);
    // % Race Time in 1st Place (fn_1_3945C): 100 x distance in first / GP and VS distance.
    const float vs = std::min(f32(0xE4), 1e9f), first = std::min(f32(0xE0), 1e9f);
    r.firstPercent = vs != 0 ? std::uint32_t(std::min(100.f * first / vs, 100.f)) : 0;
    r.hitsGiven = u32(0xD0); r.hitsTaken = u32(0xD4);
    r.ghostWins = be32(p + 0xA8); r.ghostLosses = be32(p + 0xAC);
    r.tournaments = be16(p + 0xE8);
    return r;
}

bool SaveStore::automaticDrift() const
{
    // DriftSelectPage_onActivate (PAL 0x8084E13C): 1 = manual, anything else
    // selects the automatic button.
    return current_ >= 0 && (license(unsigned(current_))[Drift] >> 6) != 1;
}
void SaveStore::setAutomaticDrift(bool automatic)
{
    if (current_ < 0) return;
    std::uint8_t& d = license(unsigned(current_))[Drift];
    const std::uint8_t v = std::uint8_t((d & 0x3F) | ((automatic ? 2u : 1u) << 6));
    if (v != d) { d = v; markDirty(); }
}

void SaveStore::markDirty()
{
    if (saving_) dirty_ = true;
}
bool SaveStore::saveNow()
{
    if (!saving_) return false;
    dirty_ = false;
    return write();
}
bool SaveStore::flushIfDirty()
{
    if (!saving_ || !dirty_) return false;
    dirty_ = false;
    return write();
}

bool SaveStore::write()
{
    if (!loaded_ || !saving_) return false;
    char main[128], backup[128], temporary[128];
    path(main, sizeof main, folder_, "rksys.dat");
    path(backup, sizeof backup, folder_, "rksys.bak");
    path(temporary, sizeof temporary, folder_, "rksys.tmp");
    makeFolders(folder_);
    put32(core_.data() + CrcOffset, crc32(core_.data(), CrcOffset));
    std::FILE* out = std::fopen(temporary, "wb");
    if (!out) { writeFailed_ = true; return false; }
    bool ok = std::fwrite(core_.data(), 1, CoreSize, out) == CoreSize;
    // A full-size file copied from a Wii or Dolphin keeps its ghost area.
    if (ok && tailSize_) {
        const char* source = fileSize(main) == long(CoreSize + tailSize_) ? main
                           : fileSize(backup) == long(CoreSize + tailSize_) ? backup : nullptr;
        std::FILE* in = source ? std::fopen(source, "rb") : nullptr;
        if (in && !std::fseek(in, long(CoreSize), SEEK_SET)) {
            static std::uint8_t chunk[0x4000]; // not on the menu loop's 64 KiB stack
            for (std::uint32_t left = tailSize_; ok && left;) {
                const std::size_t n = std::min<std::size_t>(left, sizeof chunk);
                ok = std::fread(chunk, 1, n, in) == n && std::fwrite(chunk, 1, n, out) == n;
                left -= std::uint32_t(n);
            }
        } else
            ok = false;
        if (in) std::fclose(in);
    }
    if (std::fflush(out)) ok = false;
    if (std::fclose(out)) ok = false;
    // FAT rename cannot replace a file: keep the previous file as rksys.bak.
    if (ok && exists(main)) {
        if (exists(backup) && std::remove(backup)) ok = false;
        if (ok && std::rename(main, backup)) ok = false;
    }
    if (ok && std::rename(temporary, main)) ok = false;
    if (!ok) std::remove(temporary);
    writeFailed_ = !ok;
    return ok;
}

SaveStore& saveStore()
{
    static SaveStore store;
    return store;
}
}
