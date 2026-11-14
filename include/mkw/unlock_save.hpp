#pragma once
// The unlock rules (unlocks.hpp) on the active licence of the save store.
#include <mkw/save_store.hpp>
#include <mkw/unlocks.hpp>
#include <optional>

namespace mkw::unlock {
// The current licence's flags, cup records, Time Trial bests and counters.
// No licence selected: an empty progress (nothing unlocked beyond the starters).
Progress progressOf(const SaveStore&);
// The same for any occupied licence card (licence select, before it is chosen): flags and
// cups through SaveStore, the counters and rank-1 times from the licence's RKPD bytes
// (+0xB4 races, +0xB8 battles, +0xDC0 Time Trial table), as SaveStore decodes them.
Progress progressOf(const SaveStore&, unsigned slot);
// A reward is pending on that licence (fn_1_DB3EC PAL 0x805EB7A0: unlock pages first).
bool pendingOn(const SaveStore&, unsigned slot);
// The current licence's flags (all zero without a licence).
UnlockWords flagsOf(const SaveStore&);
// Unlock page activation (fn_1_343D50): sets the reward's bit on the current
// licence and writes the save at once (retail requestSaveLicenses).
// nullopt when nothing is pending; saved=false if the write failed (the bit
// stays set in memory, as retail).
struct Activated { Notice notice; bool saved = false; };
std::optional<Activated> activateNext(SaveStore&);
// Congratulations page (fn_1_BC334): ending-seen bit, written at the next
// section change (retail markLicensesDirty).
void markEndingSeen(SaveStore&, Credits);
// TitlePage_onInit 0x8063AB6C: random title pictures once any licence saw the true ending.
bool titleVariants(const SaveStore&);
}
