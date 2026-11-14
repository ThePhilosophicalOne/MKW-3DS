#include <mkw/unlock_save.hpp>

namespace mkw::unlock {
Progress progressOf(const SaveStore& s) {
    Progress p;
    const int slot = s.current();
    if (slot < 0) return p;
    p.flags = s.unlockWords(unsigned(slot));
    for (unsigned cls = 0; cls < 4; ++cls)
        for (unsigned cup = 0; cup < 8; ++cup) p.cups[cls * 8 + cup] = s.cup(cls, cup);
    for (unsigned course = 0; course < 32; ++course) {
        const TimeRecord t = s.timeRecord(course, 0);
        p.ttBestMs[course] = t.valid ? (t.ms ? t.ms : 1) : 0;
    }
    p.races = s.raceCount();
    p.battles = s.battleCount();
    // wfcWins / ghostRaceWins: SaveStore exposes no accessor (dead services); 0.
    return p;
}

Progress progressOf(const SaveStore& s, unsigned slot) {
    Progress p;
    if (slot >= SaveStore::Licenses || !s.occupied(slot)) return p;
    p.flags = s.unlockWords(slot);
    for (unsigned cls = 0; cls < 4; ++cls)
        for (unsigned cup = 0; cup < 8; ++cup) p.cups[cls * 8 + cup] = s.cup(slot, cls, cup);
    const std::uint8_t* rkpd = s.core() + 8 + slot * 0x8CC0;
    auto be32 = [](const std::uint8_t* b) { return std::uint32_t(b[0]) << 24 | std::uint32_t(b[1]) << 16 | std::uint32_t(b[2]) << 8 | b[3]; };
    p.races = be32(rkpd + 0xB4);
    p.battles = be32(rkpd + 0xB8);
    for (unsigned course = 0; course < 32; ++course) {
        const int entry = saveSlotForCourse(course);
        if (entry < 0) continue;
        const std::uint8_t* e = rkpd + 0xDC0 + unsigned(entry) * 0x60;
        if (!(e[0x50] & 0x80)) continue;
        const std::uint32_t w = be32(e + 0x4C);
        const unsigned m = w >> 25, sec = (w >> 18) & 0x7F, ms = (w >> 8) & 0x3FF;
        const std::uint32_t t = sec < 60 && ms < 1000 ? (m * 60 + sec) * 1000 + ms : 0;
        p.ttBestMs[course] = t ? t : 1;
    }
    return p;
}

bool pendingOn(const SaveStore& s, unsigned slot) {
    return slot < SaveStore::Licenses && s.occupied(slot) && nextSection(progressOf(s, slot)).has_value();
}

UnlockWords flagsOf(const SaveStore& s) {
    const int slot = s.current();
    return slot < 0 ? UnlockWords{} : s.unlockWords(unsigned(slot));
}

std::optional<Activated> activateNext(SaveStore& s) {
    if (s.current() < 0) return std::nullopt;
    Progress p = progressOf(s);
    const auto notice = unlock::activateNext(p);
    if (!notice) return std::nullopt;
    s.setUnlocked(notice->bit);
    return Activated{*notice, s.saveNow()};
}

void markEndingSeen(SaveStore& s, Credits kind) {
    if (s.current() < 0) return;
    UnlockWords f = flagsOf(s);
    if (!unlock::markEndingSeen(f, kind)) return;
    s.setUnlocked(kind == Credits::True ? BitTrueEnding : BitNormalEnding);
    s.markDirty();
}

bool titleVariants(const SaveStore& s) { return s.anyLicenseUnlocked(BitTrueEnding); }
}
