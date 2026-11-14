// Host test: the unlock rules on the real save store (src/unlock_save.cpp + src/save_store.cpp).
// Run tools/tests/run_unlock_tests.sh.
#include <mkw/unlock_save.hpp>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
namespace fs = std::filesystem;
using namespace mkw;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

int main() {
    const fs::path dir = fs::temp_directory_path() / ("mkw-unlock-save-" + std::to_string(std::rand()));
    fs::remove_all(dir); fs::create_directories(dir);
    const std::string folder = dir.generic_string();
    {
        auto s = std::make_unique<SaveStore>();
        s->boot(folder.c_str());
        CHECK(!unlock::activateNext(*s) && unlock::flagsOf(*s) == UnlockWords{});   // no licence selected
        MiiName name{}; name[0] = u'A';
        CHECK(s->createLicense(0, name, MiiId{1}) && s->select(0));
        CHECK(!unlock::nextSection(unlock::progressOf(*s)));
        // A 50cc Star Cup win: the Special Cup (condition 4) first, then King Boo (31).
        s->recordCupResult(0, 2, 1, 0);
        const auto first = unlock::activateNext(*s);
        CHECK(first && first->saved && first->notice.condition == 4 && s->unlocked(18));
        const auto second = unlock::activateNext(*s);
        CHECK(second && second->notice.bit == 9 && s->unlocked(9) && !unlock::activateNext(*s));
        // Time Trial best at the expert threshold of Luigi Circuit (retail course 8).
        TimeRecord t; t.valid = true; t.ms = 86394; t.character = 0; t.vehicle = 0;
        s->recordTime(8, t);
        const auto p = unlock::progressOf(*s);
        CHECK(p.ttBestMs[8] == 86394 && unlock::expertGhostCount(p) == 1);
        CHECK(unlock::nextSection(p) == unlock::Section::Vehicle);   // Tiny Titan (38)
        // Licence select reads a card's RKPD bytes the same way (pendingOn, before select).
        const auto raw = unlock::progressOf(*s, 0);
        bool cupsSame = true;
        for (unsigned i = 0; i < raw.cups.size(); ++i)
            cupsSame &= raw.cups[i].played == p.cups[i].played && raw.cups[i].trophy == p.cups[i].trophy && raw.cups[i].rank == p.cups[i].rank;
        CHECK(raw.flags == p.flags && raw.ttBestMs == p.ttBestMs && raw.races == p.races && raw.battles == p.battles && cupsSame);
        CHECK(unlock::pendingOn(*s, 0) && !unlock::pendingOn(*s, 1));
        // Endings: the seen bits, title variants.
        CHECK(!unlock::titleVariants(*s));
        unlock::markEndingSeen(*s, unlock::Credits::Normal);
        CHECK(s->unlocked(unlock::BitNormalEnding) && s->dirty() && !unlock::titleVariants(*s));
        unlock::markEndingSeen(*s, unlock::Credits::True);
        CHECK(unlock::titleVariants(*s));
        s->flushIfDirty();
    }
    {   // Everything above is on the card.
        auto s = std::make_unique<SaveStore>();
        s->boot(folder.c_str());
        CHECK(s->select(0) && s->unlocked(18) && s->unlocked(9) && s->unlocked(56) && s->unlocked(57));
        CHECK(unlock::creditsSkippable(unlock::flagsOf(*s), unlock::Credits::True));
    }
    fs::remove_all(dir);
    if (failures) { std::printf("unlock_save: %d failure(s)\n", failures); return 1; }
    std::puts("unlock_save: unlock pages and ending flags on the save store, written and read back, passed");
    return 0;
}
