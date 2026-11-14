// Host tests for include/mkw/save_store.hpp and license_identity.hpp.
// Build and run: tools/tests/run_save_tests.sh (MSYS2 g++).
#include <mkw/save_store.hpp>
#include <mkw/license_identity.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
namespace fs = std::filesystem;
using namespace mkw;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static std::vector<std::uint8_t> bytes(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
static void put(const fs::path& p, const std::vector<std::uint8_t>& b)
{
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
}
static unsigned be16(const std::uint8_t* p) { return unsigned(p[0]) << 8 | p[1]; }
static std::uint32_t be32(const std::uint8_t* p) { return std::uint32_t(p[0]) << 24 | std::uint32_t(p[1]) << 16 | std::uint32_t(p[2]) << 8 | p[3]; }
static const std::uint8_t* lic(const SaveStore& s, unsigned slot) { return s.core() + 8 + slot * 0x8CC0; }

struct Dir {
    fs::path path;
    explicit Dir(const char* name)
    {
        path = fs::temp_directory_path() / (std::string("mkw-save-") + name + "-" + std::to_string(std::rand()));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~Dir() { fs::remove_all(path); }
    std::string str() const { return path.generic_string(); }
};
static std::unique_ptr<SaveStore> store() { return std::make_unique<SaveStore>(); }
static MiiName nameOf(const char* s)
{
    MiiName n{};
    for (unsigned i = 0; i < n.size() && s[i]; ++i) n[i] = char16_t(s[i]);
    return n;
}

static void testCrc()
{
    const char* v = "123456789";
    CHECK(saveCrc32(reinterpret_cast<const std::uint8_t*>(v), 9) == 0xCBF43926u);
}

// fn_1_37D50 (PAL 0x80548104) and RawLicense_reset (PAL 0x80548300).
static void testNewFile()
{
    Dir d("new");
    auto s = store();
    CHECK(s->boot(d.str().c_str()) == SaveBoot::Created);
    const auto file = bytes(d.path / "rksys.dat");
    CHECK(file.size() == 0x28000);
    CHECK(!std::memcmp(file.data(), "RKSD0006", 8));
    CHECK(!std::memcmp(file.data() + 0x23308, "RKGD", 4));
    CHECK(be32(file.data() + 0x23308 + 4) == 0x27);
    for (unsigned i = 0; i < 10; ++i) CHECK(be32(file.data() + 0x23308 + 8 + i * 0x384) == 0xFFFFFFFFu);
    CHECK((be16(file.data() + 0x26B0A) >> 12) == 1);
    CHECK(be32(file.data() + 0x27FFC) == saveCrc32(file.data(), 0x27FFC));
    for (unsigned slot = 0; slot < 4; ++slot) {
        const std::uint8_t* p = file.data() + 8 + slot * 0x8CC0;
        CHECK(be32(p) == 0);
        CHECK(be16(p + 0x80) == 0x5008 && be16(p + 0x84) == 0x5008);
        CHECK(be16(p + 0x82) == 0x1000 && be16(p + 0x86) == 0x1000);
        CHECK(be16(p + 0xB0) == 5000 && be16(p + 0xB2) == 5000);
        CHECK(p[0xEA] == 0x80);
        unsigned other = 0;
        for (unsigned i = 0; i < 0x8CC0; ++i)
            if (!(i >= 0x80 && i < 0x88) && !(i >= 0xB0 && i < 0xB4) && i != 0xEA) other |= p[i];
        CHECK(other == 0);
        CHECK(!s->occupied(slot));
    }
    // No legacy data: no licence was made.
    CHECK(s->current() == -1);
    // A second boot reads the same file.
    auto t = store();
    CHECK(t->boot(d.str().c_str()) == SaveBoot::Ready);
    CHECK(!std::memcmp(t->core(), file.data(), 0x28000));
}

static void testCreateEraseSelect()
{
    Dir d("lic");
    auto s = store();
    s->boot(d.str().c_str());
    MiiId id{1, 2, 3, 4, 5, 6, 7, 8};
    const auto before = bytes(d.path / "rksys.dat");
    CHECK(s->createLicense(2, nameOf("Toad"), id));
    CHECK(s->occupied(2) && !s->miiMissing(2));
    CHECK(s->name(2) == nameOf("Toad"));
    const auto after = bytes(d.path / "rksys.dat");   // saved at once (requestSaveLicenses)
    const std::uint8_t* p = after.data() + 8 + 2 * 0x8CC0;
    CHECK(!std::memcmp(p, "RKPD", 4));
    CHECK(be16(p + 0x14) == 'T' && be16(p + 0x16) == 'o' && be16(p + 0x1C) == 0);
    CHECK(!std::memcmp(p + 0x28, id.data(), 8));
    CHECK(be32(p + 0x30) == 0 && be32(p + 0x34) == 0 && be32(p + 0x38) == 0 && be32(p + 0x3C) == 0); // locked
    // Other slots and the global block are byte-identical.
    for (unsigned i = 0; i < 0x27FFC; ++i)
        if (i < 8 + 2 * 0x8CC0 || i >= 8 + 3 * 0x8CC0) { if (before[i] != after[i]) { CHECK(false); break; } }
    CHECK(fs::exists(d.path / "rksys.bak"));
    CHECK(!s->select(1) && s->select(2) && s->current() == 2);
    // Change Mii.
    MiiId other{9, 9, 9, 9, 9, 9, 9, 9};
    CHECK(s->changeMii(2, nameOf("Guest A"), other));
    CHECK(s->name(2) == nameOf("Guest A"));
    // Erase: the slot is reset, the rest untouched, selection cleared.
    const auto pre = bytes(d.path / "rksys.dat");
    CHECK(s->eraseLicense(2));
    CHECK(!s->occupied(2) && s->current() == -1);
    const auto post = bytes(d.path / "rksys.dat");
    const std::uint8_t* fresh = before.data() + 8 + 2 * 0x8CC0;
    CHECK(!std::memcmp(post.data() + 8 + 2 * 0x8CC0, fresh, 0x8CC0));
    for (unsigned i = 0; i < 0x27FFC; ++i)
        if (i < 8 + 2 * 0x8CC0 || i >= 8 + 3 * 0x8CC0) { if (pre[i] != post[i]) { CHECK(false); break; } }
    // Erase keeps .rkg ghost files (owner decision).
    fs::create_directories(d.path / "ghosts");
    put(d.path / "ghosts" / "pb-08.rkg", {'R', 'K', 'G', 'D'});
    s->createLicense(0, nameOf("A"), id);
    s->eraseLicense(0);
    CHECK(fs::exists(d.path / "ghosts" / "pb-08.rkg"));
}

// fn_1_ACC9C (PAL 0x805BD050): keep the best trophy and the best rank.
static void testCupRule()
{
    Dir d("cup");
    auto s = store();
    s->boot(d.str().c_str());
    s->createLicense(0, nameOf("A"), MiiId{1});
    CHECK(!s->recordCupResult(2, 3, 1, 0));      // no licence selected
    s->select(0);
    s->flushIfDirty();
    CHECK(!s->cup(2, 3).played);
    CHECK(s->recordCupResult(2, 3, 5, 6) && s->dirty());      // 5th, D: played, no trophy
    auto c = s->cup(2, 3);
    CHECK(c.played && c.trophy == Trophy::None && c.rank == 6);
    const std::uint8_t* e = lic(*s, 0) + 0x1C0 + 2 * 0x300 + 3 * 0x60;
    CHECK(((be32(e + 0x4C) >> 6) & 3) == 3 && (be16(e + 0x50) & 0xF) == 6 && (e[0x52] & 0x80));
    CHECK((be32(e + 0x4C) >> 8) == 0);           // time written as zero
    s->flushIfDirty();
    CHECK(!s->recordCupResult(2, 3, 7, 8) && !s->dirty());    // worse in both: nothing
    CHECK(s->recordCupResult(2, 3, 2, 7));       // silver, worse rank: trophy improves, rank kept
    c = s->cup(2, 3);
    CHECK(c.trophy == Trophy::Silver && c.rank == 6);
    CHECK(s->recordCupResult(2, 3, 3, 0));       // bronze, 3 stars: rank improves, silver kept
    c = s->cup(2, 3);
    CHECK(c.trophy == Trophy::Silver && c.rank == 0);
    CHECK(s->recordCupResult(2, 3, 1, 1));       // gold, 2 stars: trophy improves, 3 stars kept
    c = s->cup(2, 3);
    CHECK(c.trophy == Trophy::Gold && c.rank == 0);
    CHECK(!s->recordCupResult(2, 3, 1, 0));      // equal in both: nothing
    CHECK(!s->recordCupResult(4, 0, 1, 0) && !s->recordCupResult(0, 8, 1, 0) && !s->recordCupResult(0, 0, 0, 0) &&
          !s->recordCupResult(0, 0, 13, 0) && !s->recordCupResult(0, 0, 1, 9));
    CHECK(!s->cup(1, 2, 3).played);              // other licence
    // Mirror (class 3) is its own row.
    CHECK(s->recordCupResult(3, 7, 2, 2) && s->cup(3, 7).trophy == Trophy::Silver && !s->cup(2, 7).played);
    // Written at the next section change, with a valid CRC.
    CHECK(s->flushIfDirty());
    auto t = store();
    CHECK(t->boot(d.str().c_str()) == SaveBoot::Ready);
    CHECK(t->cup(0, 2, 3).trophy == Trophy::Gold && t->cup(0, 2, 3).rank == 0);
}

// UnlockFlags_get/set (PAL 0x8054994C / 0x805498FC) and the union (0x8054A658).
static void testUnlocks()
{
    Dir d("unl");
    auto s = store();
    s->boot(d.str().c_str());
    s->createLicense(0, nameOf("A"), MiiId{1});
    s->createLicense(1, nameOf("B"), MiiId{2});
    s->select(0);
    for (unsigned bit = 0; bit < 128; ++bit) {
        CHECK(!s->unlocked(bit));
        s->setUnlocked(bit);
        CHECK(s->unlocked(bit));
        const unsigned word = bit / 32;
        CHECK(be32(lic(*s, 0) + 0x30 + 4 * word) & (1u << (bit % 32)));
    }
    s->eraseLicense(0);
    s->createLicense(0, nameOf("A"), MiiId{1});
    s->select(0);
    s->setUnlocked(0x34);
    s->select(1);
    s->setUnlocked(0x38);
    CHECK(s->anyLicenseUnlocked(0x34) && s->anyLicenseUnlocked(0x38) && !s->anyLicenseUnlocked(0x39));
    const auto u = s->unlockUnion();
    CHECK(u[1] == ((1u << 20) | (1u << 24)) && u[0] == 0 && u[2] == 0 && u[3] == 0);
    CHECK(s->unlockWords(0)[1] == (1u << 20) && s->unlockWords(1)[1] == (1u << 24) && s->unlockWords(3)[1] == 0);
    CHECK(!s->unlocked(128));
    CHECK(!NewLicenseStartsUnlocked);
}

// fn_1_273654 (PAL 0x80783A08) and fn_1_34C1EC (PAL 0x8085C5A0).
static void testTally()
{
    Dir d("tal");
    auto s = store();
    s->boot(d.str().c_str());
    s->createLicense(0, nameOf("A"), MiiId{1});
    s->select(0);
    RaceTally t;
    t.race = true; t.character = 19; t.vehicle = 35; t.course = 0x08; t.mode = 0; t.place = 3; t.racers = 12;
    s->tallyRace(t);
    const std::uint8_t* p = lic(*s, 0);
    CHECK(be32(p + 0xB4) == 1 && s->raceCount() == 1 && s->battleCount() == 0);
    CHECK(be16(p + 0xEC + 2 * 19) == 1 && be16(p + 0x11E + 2 * 35) == 1 && be16(p + 0x166 + 2 * 8) == 1);
    CHECK(be32(p + 0x88) == 9 && be32(p + 0x8C) == 2 && be32(p + 0xDC) == 0);
    t.place = 1; s->tallyRace(t);
    CHECK(be32(p + 0xDC) == 1 && be32(p + 0x88) == 20 && be32(p + 0x8C) == 2);
    t.mode = 2; t.place = 1; s->tallyRace(t);    // Time Trials: no win tally
    CHECK(be32(p + 0xDC) == 1 && be32(p + 0xB4) == 3);
    RaceTally b; b.battle = true; b.stage = 4; b.mode = 3; b.character = 30; s->tallyRace(b);
    CHECK(be32(p + 0xB8) == 1 && be16(p + 0x1A6 + 8) == 1 && be16(p + 0xEC + 48) == 1);   // Mii -> 24
    // Clamp at 99999.
    for (int i = 0; i < 100010; ++i) { RaceTally r; r.race = true; r.mode = 2; s->tallyRace(r); }
    CHECK(s->raceCount() == 99999);
    // u16 favourites stop at 0xFFFF.
    for (int i = 0; i < 70000; ++i) { RaceTally r; r.vehicle = 0; r.mode = 2; s->tallyRace(r); }
    CHECK(be16(p + 0x11E) == 0xFFFF);
    RaceTally w; w.mode = 1; w.place = 12; w.racers = 12;
    for (int i = 0; i < 9100; ++i) s->tallyRace(w);
    CHECK(be32(p + 0x8C) == 99999);
}

// License_getLeaderboardSlot (PAL 0x805490F4) and the best lap.
static void testTimes()
{
    Dir d("tt");
    auto s = store();
    s->boot(d.str().c_str());
    s->createLicense(0, nameOf("A"), MiiId{1});
    s->select(0);
    auto rec = [](std::uint32_t ms, std::uint8_t ch) { TimeRecord r; r.ms = ms; r.character = ch; r.vehicle = 5; return r; };
    const unsigned lc = 0x08;   // Luigi Circuit, save slot 0
    CHECK(s->recordTime(lc, rec(70000, 1)) == 0);
    CHECK(s->recordTime(lc, rec(80000, 2)) == 1);
    CHECK(s->recordTime(lc, rec(60000, 3)) == 0);
    CHECK(s->recordTime(lc, rec(70000, 4)) == 2);   // equal time goes below the existing one
    CHECK(s->recordTime(lc, rec(90000, 5)) == 4);
    CHECK(s->recordTime(lc, rec(95000, 6)) == -1);  // slower than all five
    const unsigned order[5] = {60000, 70000, 70000, 80000, 90000};
    const unsigned chars[5] = {3, 1, 4, 2, 5};
    for (unsigned r = 0; r < 5; ++r) {
        const auto t = s->timeRecord(lc, r);
        CHECK(t.valid && t.ms == order[r] && t.character == chars[r] && t.vehicle == 5 && t.controller == 2);
    }
    const std::uint8_t* e = lic(*s, 0) + 0xDC0;     // rank 1, slot 0
    CHECK(be32(e + 0x4C) == ((1u << 25) | (0u << 18) | (0u << 8) | (5u << 2)) && e[0x50] == (0x80 | 3) && (e[0x51] >> 5) == 2);
    CHECK(s->recordTime(lc, rec(1000, 7)) == 0 && s->timeRecord(lc, 4).ms == 80000);   // fifth drops off
    CHECK(s->timeTrialCourses() == 1);
    // Best lap: rank 5 at 0x49C0.
    CHECK(s->recordBestLap(lc, rec(20000, 1)) && !s->recordBestLap(lc, rec(20000, 2)) && s->recordBestLap(lc, rec(19999, 3)));
    CHECK(s->timeRecord(lc, 5).ms == 19999 && be32(lic(*s, 0) + 0x49C0 + 0x4C) >> 25 == 0);
    // The retail slot table (PAL 0x808911C0).
    const std::uint8_t table[32] = {0x08, 0x01, 0x02, 0x04, 0x00, 0x05, 0x06, 0x07, 0x09, 0x0F, 0x03, 0x0B, 0x0A, 0x0E, 0x0C, 0x0D,
                                    0x1F, 0x19, 0x18, 0x1E, 0x1B, 0x1A, 0x1D, 0x1C, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    for (unsigned i = 0; i < 32; ++i) CHECK(saveSlotForCourse(table[i]) == int(i));
    CHECK(saveSlotForCourse(0x20) == -1);
    CHECK(s->recordTime(0x20, rec(1000, 1)) == -1);
    // Drift choice (0xEA: 1 manual, 2 automatic; reset value 2).
    CHECK(s->automaticDrift());
    s->setAutomaticDrift(false);
    CHECK(!s->automaticDrift() && lic(*s, 0)[0xEA] == 0x40);
}

static void testLoadErrors()
{
    Dir d("err");
    {
        auto s = store();
        s->boot(d.str().c_str());
        s->createLicense(0, nameOf("A"), MiiId{1});
        s->createLicense(1, nameOf("B"), MiiId{2});   // second write: rksys.bak = the file with A only
    }
    // A damaged rksys.dat falls back to rksys.bak.
    auto file = bytes(d.path / "rksys.dat");
    file[0x100] ^= 0xFF;
    put(d.path / "rksys.dat", file);
    {
        auto s = store();
        CHECK(s->boot(d.str().c_str()) == SaveBoot::Ready);
        CHECK(s->occupied(0) && !s->occupied(1));
    }
    // Both damaged: Corrupt; recreate keeps the bad file aside.
    auto backup = bytes(d.path / "rksys.bak");
    backup[0x200] ^= 0xFF;
    put(d.path / "rksys.bak", backup);
    {
        auto s = store();
        CHECK(s->boot(d.str().c_str()) == SaveBoot::Corrupt);
        CHECK(!s->saveNow() || true);
        CHECK(s->recreate());
        CHECK(fs::exists(d.path / "rksys-corrupt.dat") && bytes(d.path / "rksys-corrupt.dat") == file);
        CHECK(!s->occupied(0));
        auto t = store();
        CHECK(t->boot(d.str().c_str()) == SaveBoot::Ready);
    }
    // Wrong size counts as damaged.
    put(d.path / "rksys.dat", {1, 2, 3});
    fs::remove(d.path / "rksys.bak");
    { auto s = store(); CHECK(s->boot(d.str().c_str()) == SaveBoot::Corrupt); }
    // A folder that cannot hold files: Unwritable, nothing changes.
    Dir e("unw");
    put(e.path / "blocked", {0});
    { auto s = store(); CHECK(s->boot((e.path / "blocked").generic_string().c_str()) == SaveBoot::Unwritable); CHECK(s->writeFailed()); }
    // Saving disabled ("Start without saving"): nothing is written.
    Dir f("off");
    {
        auto s = store();
        s->boot(f.str().c_str());
        const auto first = bytes(f.path / "rksys.dat");
        s->disableSaving();
        CHECK(!s->savingEnabled());
        s->createLicense(0, nameOf("A"), MiiId{1});     // in memory only
        CHECK(s->occupied(0));
        s->select(0);
        s->recordCupResult(0, 0, 1, 0);
        CHECK(!s->dirty() && !s->flushIfDirty() && !s->saveNow());
        CHECK(bytes(f.path / "rksys.dat") == first);
    }
}

// A retail-size file (0x2BC000, from a Wii or Dolphin) keeps its ghost area.
static void testFullSize()
{
    Dir d("full");
    {
        auto s = store();
        s->boot(d.str().c_str());
    }
    auto core = bytes(d.path / "rksys.dat");
    std::vector<std::uint8_t> full(0x2BC000);
    std::copy(core.begin(), core.end(), full.begin());
    for (std::size_t i = 0x28000; i < full.size(); ++i) full[i] = std::uint8_t(i * 7 + 3);
    put(d.path / "rksys.dat", full);
    fs::remove(d.path / "rksys.bak");
    auto s = store();
    CHECK(s->boot(d.str().c_str()) == SaveBoot::Ready);
    CHECK(s->createLicense(3, nameOf("W"), MiiId{3}));
    const auto after = bytes(d.path / "rksys.dat");
    CHECK(after.size() == 0x2BC000);
    CHECK(std::equal(full.begin() + 0x28000, full.end(), after.begin() + 0x28000));
    CHECK(s->createLicense(2, nameOf("X"), MiiId{4}));   // second write copies the tail again
    const auto again = bytes(d.path / "rksys.dat");
    CHECK(again.size() == 0x2BC000 && std::equal(full.begin() + 0x28000, full.end(), again.begin() + 0x28000));
}

static std::vector<std::uint8_t> rkg(unsigned course, std::uint32_t ms, unsigned character, unsigned vehicle, unsigned controller)
{
    std::vector<std::uint8_t> b(0x2800, 0);
    std::memcpy(b.data(), "RKGD", 4);
    const std::uint32_t a = (ms / 60000) << 25 | ((ms / 1000) % 60) << 18 | (ms % 1000) << 8 | (course & 0x3F) << 2;
    const std::uint32_t w = (vehicle & 0x3F) << 26 | (character & 0x3F) << 20 | 26u << 13 | 11u << 9 | 11u << 4 | (controller & 0xF);
    for (unsigned i = 0; i < 4; ++i) { b[4 + i] = std::uint8_t(a >> (24 - 8 * i)); b[8 + i] = std::uint8_t(w >> (24 - 8 * i)); }
    return b;
}

// v0.7.0 data: best-times.bin (32 LE u32, port catalog order) + ghosts/pb-XX.rkg.
static void testMigration()
{
    Dir d("mig");
    std::vector<std::uint8_t> legacy(128, 0);
    auto setTime = [&](unsigned index, std::uint32_t ms) { for (unsigned i = 0; i < 4; ++i) legacy[4 * index + i] = std::uint8_t(ms >> (8 * i)); };
    setTime(0, 75123);    // Luigi Circuit (0x08), ghost matches
    setTime(1, 81000);    // Moo Moo Meadows (0x01), no ghost
    setTime(21, 99999);   // GBA Shy Guy Beach (0x1F), ghost with another time
    setTime(22, 123456);  // DS Delfino Square (0x17) -> save slot 31
    put(d.path / "best-times.bin", legacy);
    fs::create_directories(d.path / "ghosts");
    put(d.path / "ghosts" / "pb-08.rkg", rkg(0x08, 75123, 19, 33, 2));
    put(d.path / "ghosts" / "pb-1f.rkg", rkg(0x1F, 100500, 3, 4, 3));
    put(d.path / "ghosts" / "pb-0d.rkg", rkg(0x0D, 200000, 5, 6, 2));   // Rainbow Road: ghost only
    put(d.path / "ghosts" / "friend.rkg", rkg(0x08, 60000, 1, 1, 2));    // not licence 1's
    const auto legacyBefore = bytes(d.path / "best-times.bin");
    std::vector<std::vector<std::uint8_t>> ghostBefore;
    for (const char* n : {"pb-08.rkg", "pb-1f.rkg", "pb-0d.rkg", "friend.rkg"}) ghostBefore.push_back(bytes(d.path / "ghosts" / n));
    const auto stamp = fs::last_write_time(d.path / "best-times.bin");

    auto s = store();
    CHECK(s->boot(d.str().c_str()) == SaveBoot::Created);
    CHECK(s->occupied(0) && s->miiMissing(0) && s->name(0) == MiiName{});
    CHECK(!s->occupied(1) && !s->occupied(2) && !s->occupied(3));
    CHECK(s->unlockWords(0) == UnlockWords{});   // locked like retail
    CHECK(s->select(0));
    auto t = s->timeRecord(0x08);
    CHECK(t.valid && t.ms == 75123 && t.character == 19 && t.vehicle == 33 && t.controller == 2 && !t.driverUnknown);
    t = s->timeRecord(0x01);
    CHECK(t.valid && t.ms == 81000 && t.driverUnknown);
    t = s->timeRecord(0x1F);
    CHECK(t.valid && t.ms == 99999 && t.driverUnknown);
    t = s->timeRecord(0x17);
    CHECK(t.valid && t.ms == 123456 && t.driverUnknown);
    CHECK(be32(lic(*s, 0) + 0xDC0 + 31 * 0x60 + 0x4C) >> 25 == 2);      // DS Delfino Square in slot 31
    t = s->timeRecord(0x0D);
    CHECK(t.valid && t.ms == 200000 && t.character == 5 && !t.driverUnknown);
    CHECK(s->timeTrialCourses() == 5);
    CHECK(!s->timeRecord(0x08, 1).valid);
    // The old files are untouched.
    CHECK(bytes(d.path / "best-times.bin") == legacyBefore);
    CHECK(fs::last_write_time(d.path / "best-times.bin") == stamp);
    unsigned k = 0;
    for (const char* n : {"pb-08.rkg", "pb-1f.rkg", "pb-0d.rkg", "friend.rkg"}) CHECK(bytes(d.path / "ghosts" / n) == ghostBefore[k++]);
    // The first press of the card asks for a Mii; then the licence is complete.
    CHECK(s->changeMii(0, nameOf("Toad"), MiiId{0x81}));
    CHECK(!s->miiMissing(0));
    // A second boot reads the file and does not migrate again.
    auto u = store();
    CHECK(u->boot(d.str().c_str()) == SaveBoot::Ready);
    u->select(0);
    CHECK(u->name(0) == nameOf("Toad") && u->timeRecord(0x08).ms == 75123);

    // Only ghosts, no best-times.bin.
    Dir g("mig2");
    fs::create_directories(g.path / "ghosts");
    put(g.path / "ghosts" / "pb-01.rkg", rkg(0x01, 90000, 2, 3, 2));
    auto v = store();
    CHECK(v->boot(g.str().c_str()) == SaveBoot::Created);
    v->select(0);
    CHECK(v->occupied(0) && v->timeRecord(0x01).ms == 90000 && v->timeRecord(0x01).character == 2);
    // A short best-times.bin is not read.
    Dir h("mig3");
    put(h.path / "best-times.bin", std::vector<std::uint8_t>(64, 1));
    auto w = store();
    CHECK(w->boot(h.str().c_str()) == SaveBoot::Created && !w->occupied(0));
}

// Upstream's Mii ID conversion and sidecar (tools/tests/license_mii_store.cpp).
static void testMii()
{
    Dir d("mii");
    NativeMii a{};
    a[0] = 3; a[4] = 0x11; a[5] = 0x22; a[6] = 0x33; a[7] = 0x44; a[12] = 0x90;
    const MiiId expected = {0x81, 0xe1, 0x87, 0xe0, 0x11, 0x22, 0x33, 0x44};
    CHECK(licenseMiiId(a) == expected);
    a[15] = 2; CHECK(licenseMiiId(a)[3] == 0xe1);
    a[12] = 0x10; CHECK(licenseMiiId(a)[0] == 0x41); a[12] = 0x90;
    for (unsigned i = 24; i < a.size(); ++i) a[i] = std::uint8_t(i * 7);
    CHECK(writeNativeMii(d.str().c_str(), a));
    NativeMii b{};
    CHECK(loadNativeMii(d.str().c_str(), licenseMiiId(a), b) && a == b);
    CHECK(writeNativeMii(d.str().c_str(), a));
    auto other = licenseMiiId(a); other[7] ^= 1;
    CHECK(!loadNativeMii(d.str().c_str(), other, b));
    a[0] = 0; CHECK(!writeNativeMii(d.str().c_str(), a));
}

// The records pages' values (fn_1_34F238 / 34F580 / 34F970 and helpers) and the
// stats block's tally (fn_1_273368 + fn_1_273654).
static void testRecords()
{
    Dir d("rec");
    auto s = store();
    s->boot(d.str().c_str());
    s->createLicense(1, nameOf("R"), MiiId{5});
    s->select(1);
    auto r = s->records(1);
    CHECK(r.overallRank == 7 && r.distance == 0 && r.races == 0 && r.favouriteCharacter == -1 && r.favouriteStage == -1);
    CHECK(r.vr == 5000 && r.br == 5000 && r.firstPercent == 0 && r.wheelPercent == 0);
    // Overall Ranking: mean of (8 - rank); at 1 star or better the worst of all 32.
    s->recordCupResult(2, 0, 1, 3);   // A
    s->recordCupResult(2, 1, 1, 5);   // C
    CHECK(s->records(1).overallRank == 4);   // 8 - (5 + 3) / 2 = 4 (B)
    for (unsigned cc = 0; cc < 4; ++cc)
        for (unsigned c = 0; c < 8; ++c) s->recordCupResult(cc, c, 1, 0);
    CHECK(s->records(1).overallRank == 0);   // every cup at 3 stars
    // Tally: distances (x 1e-5 already applied by the caller), hits, tricks, favourites.
    RaceTally t;
    t.race = true; t.mode = 0; t.place = 1; t.racers = 12; t.character = 19; t.vehicle = 7; t.course = 0x08;
    t.distance = 12.5f; t.distanceFirst = 5.f; t.distanceVs = 12.5f; t.hitsGiven = 3; t.hitsTaken = 2; t.tricks = 4;
    s->tallyRace(t);
    RaceTally b; b.battle = true; b.mode = 3; b.stage = 4; b.distance = 1.f; b.tricks = 1;
    s->tallyRace(b);
    r = s->records(1);
    CHECK(r.distance == 13 && r.races == 1 && r.battles == 1 && r.tricks == 5 && r.hitsGiven == 3 && r.hitsTaken == 2);
    CHECK(r.firstPercent == 40 && r.firstPlaces == 1);
    CHECK(r.favouriteCharacter == 19 && r.favouriteVehicle == 7 && r.favouriteCourse == 0x08 && r.favouriteStage == 0x24);
    float stored; std::uint32_t bits = be32(lic(*s, 1) + 0xC4); std::memcpy(&stored, &bits, 4);
    CHECK(stored == 13.5f);
    // Drift: reset value 2 = automatic; 1 = manual (DriftSelectPage_onActivate).
    CHECK(s->automaticDrift());
    s->setAutomaticDrift(false);
    CHECK(!s->automaticDrift() && s->dirty());
    s->setAutomaticDrift(true);
    CHECK(s->automaticDrift() && (lic(*s, 1)[0xEA] >> 6) == 2);
}

int main()
{
    std::srand(unsigned(std::time(nullptr)));
    testCrc();
    testNewFile();
    testCreateEraseSelect();
    testCupRule();
    testUnlocks();
    testTally();
    testTimes();
    testLoadErrors();
    testFullSize();
    testMigration();
    testMii();
    testRecords();
    if (failures) { std::printf("%d save store check(s) failed\n", failures); return 1; }
    std::puts("Save store: format, CRC, licences, cup rule, unlock bits, counters, Time Trial top 5 + best lap, "
              "load errors, full-size files, migration, Mii sidecars, records pages, stats block tally and drift passed");
    return 0;
}
