#include <mkw/unlocks.hpp>
#include <mkw/unlock_tables.hpp>
#include <algorithm>

namespace mkw::unlock {
namespace {
constexpr unsigned NoBit = ~0u;
// Message ids (Present_U / MenuSingle_U Menu.bmg) built by the retail helpers.
std::uint16_t className(unsigned engineClass) { return std::uint16_t(0xD53 + engineClass); }  // 50cc..Mirror
std::uint16_t cupName(unsigned cup) { return std::uint16_t(0x23F0 + cup); }                   // fn_1_323284 0x80833638
std::uint16_t characterName(unsigned c) { return std::uint16_t(9000 + c); }                   // fn_1_3233C0 0x80833774
std::uint16_t vehicleName(unsigned v) { return std::uint16_t(v < 18 ? 0x238C + v : 0x23AC + v); } // fn_1_32312C 0x808334E0

unsigned capped(std::uint32_t v) { return std::min<std::uint32_t>(v, 99999); }
bool cupsStar(const Progress& p, unsigned cls, unsigned first, unsigned count) {
    for (unsigned c = first; c < first + count; ++c)
        if (!starOrBetter(p.cup(cls, c))) return false;
    return true;
}
bool gold(const Progress& p, unsigned cls, unsigned cup) { return hasGold(p.cup(cls, cup)); }
bool trophy(const Progress& p, unsigned cls, unsigned cup) { return hasTrophy(p.cup(cls, cup)); }
bool charLocked(const Progress& p, unsigned c) { return !characterUnlocked(p.flags, c); }
bool vehLocked(const Progress& p, unsigned v) { return !vehicleUnlocked(p.flags, v); }
bool flagClear(const Progress& p, unsigned bit) { return !flag(p.flags, bit); }

// fn_1_3528E8 0x80862C9C: the character a reward bit unlocks.
int characterOfBit(unsigned bit) {
    constexpr int t[14] = {5, 13, 4, 12, 18, 20, 15, 17, 21, 19, 22, 23, 0x18, 0x1A};
    return bit < 14 ? t[bit] : -1;
}
// fn_1_3529A4 0x80862D58: the vehicle a reward bit unlocks or shows (30/31: the
// karts-and-bikes rewards show a Standard Bike M / Standard Kart M).
int vehicleOfBit(unsigned bit) {
    switch (bit) {
    case 30: return 0x13; case 31: return 1;
    case 34: return 9;  case 35: return 12; case 36: return 15; case 37: return 10; case 38: return 13;
    case 39: return 16; case 40: return 11; case 41: return 14; case 42: return 17; case 43: return 27;
    case 44: return 30; case 45: return 33; case 46: return 28; case 47: return 31; case 48: return 34;
    case 49: return 29; case 50: return 32; case 51: return 35;
    default: return -1;
    }
}
// fn_1_34478C 0x80854B40: the driver sitting on a vehicle reward's model.
int driverForVehicle(int v) {
    switch (v) {
    case 9: case 12: case 15: case 27: case 30: case 33: return 6;   // Baby Mario
    case 11: return 3;                                               // Bowser
    case 13: return 16;                                              // Peach
    case 14: return 9;                                               // Donkey Kong
    case 17: case 29: case 32: case 35: return 2;                    // Waluigi
    default: return 0;                                               // Mario (10, 16, 28, 31, 34, others)
    }
}
// fn_1_344BF0's mapping, for a main condition and for a race-route bit.
Section sectionOfCondition(unsigned id) {
    if (id <= 16) return Section::Course;
    if (id <= 31) return Section::Driver;
    if (id <= 34) return Section::Mii;
    if (id <= 36) return Section::Driver;
    return Section::Vehicle;
}
std::optional<Section> sectionOfRaceBit(unsigned bit) {
    if (bit >= 34 && bit <= 51) return Section::Vehicle;
    if (bit < 12) return Section::Driver;
    if (bit == 12 || bit == 13) return Section::Mii;
    if (bit == BitMirror) return Section::Course;
    return std::nullopt;
}
int firstCondition(const Progress& p) {
    for (unsigned id = 0; id < ConditionCount; ++id)
        if (conditionMet(p, id)) return int(id);
    return -1;
}
int firstRaceBit(const Progress& p) {
    for (unsigned bit = 0; bit < RaceRouteBits; ++bit)
        if (raceRouteMet(p, bit)) return int(bit);
    return -1;
}
// fn_1_344A4C: headline for a character or vehicle reward.
void personHeadline(Notice& n, int character, int vehicle) {
    if (character < 0) {
        if (vehicle < 0) return;
        const unsigned w = vehicleWeight(unsigned(vehicle));
        n.headline = std::uint16_t((vehicle < 18 ? 0x1F44 : 0x1F47) + w);
        n.headlineArg[0] = vehicleName(unsigned(vehicle));
        return;
    }
    if (isMiiOutfitA(unsigned(character))) { n.headline = 0x1F43; n.headlineArg[0] = 0x235C; }
    else if (isMiiOutfitB(unsigned(character))) { n.headline = 0x1F43; n.headlineArg[0] = 0x235D; }
    else { n.headline = 0x1F42; n.headlineArg[0] = characterName(unsigned(character)); }
}
// fn_1_34478C: the model on the page.
void models(Notice& n, int character, int vehicle) {
    if (character < 0 && vehicle >= 0) { n.modelVehicle = vehicle; n.modelCharacter = driverForVehicle(vehicle); }
    else n.modelCharacter = character;
}
}

bool flag(const UnlockWords& w, unsigned bit) { return bit < 128 && (w[bit / 32] >> (bit % 32) & 1u); }
void setFlag(UnlockWords& w, unsigned bit) { if (bit < 128) w[bit / 32] |= 1u << (bit % 32); }

bool isMii(unsigned c) { return c >= 0x18 && c <= 0x29; }
bool isMiiOutfitA(unsigned c) { return c - 0x18 < 2 || c - 0x1E < 2 || c - 0x24 < 2; }
bool isMiiOutfitB(unsigned c) { return c - 0x1A < 2 || c - 0x20 < 2 || c - 0x26 < 2; }

bool characterUnlocked(const UnlockWords& f, unsigned c) {
    if (isMiiOutfitA(c)) return flag(f, 12);
    if (isMiiOutfitB(c)) return flag(f, 13);
    if (c < unlock_tables::CharacterBit.size()) {
        const int bit = unlock_tables::CharacterBit[c];
        return bit < 0 || flag(f, unsigned(bit));
    }
    return true;   // the remaining Mii outfit and unknown ids: retail default 1
}
bool vehicleUnlocked(const UnlockWords& f, unsigned v) {
    if (v >= unlock_tables::VehicleBit.size()) return true;
    const int bit = unlock_tables::VehicleBit[v];
    return bit < 0 || flag(f, unsigned(bit));
}
bool vehicleAllowed(const UnlockWords& f, unsigned v, VehicleKind kind) {
    if (kind != VehicleKind::Bikes && v < 18) return vehicleUnlocked(f, v);
    if (kind != VehicleKind::Karts && v >= 18 && v < 36) return vehicleUnlocked(f, v);
    return false;
}
bool cupUnlocked(const UnlockWords& f, unsigned cup, unsigned cls, bool mirror) {
    if (mirror) cls = 3;
    cls &= 3;
    switch (cup) {
    case 2: return flag(f, 14 + cls);
    case 3: return flag(f, 18 + cls);
    case 6: return flag(f, 22 + cls);
    case 7: return flag(f, 26 + cls);
    default: return cup < 8;
    }
}
bool cupOpen(const UnlockWords& f, unsigned cup, bool grandPrix, unsigned cls, bool mirror) {
    if (grandPrix) return cupUnlocked(f, cup, cls, mirror);
    return cupUnlocked(f, cup, 0, false) || cupUnlocked(f, cup, 1, false) || cupUnlocked(f, cup, 2, false) || cupUnlocked(f, cup, 2, true);
}
bool mirrorUnlocked(const UnlockWords& f) { return flag(f, BitMirror); }
bool mixedTypesUnlocked(const UnlockWords& f, unsigned cls) {
    return cls == 0 ? flag(f, BitMixed50) : cls == 1 ? flag(f, BitMixed100) : false;
}
bool anyMixedTypesUnlocked(const UnlockWords& f) { return flag(f, BitMixed50) || flag(f, BitMixed100); }

bool hasTrophy(const CupRecord& r) { return r.played && r.trophy != Trophy::None; }
bool hasGold(const CupRecord& r) { return r.played && r.trophy == Trophy::Gold; }
bool starOrBetter(const CupRecord& r) { return r.played && r.rank <= 2; }

bool expertGhostAppeared(const Progress& p, unsigned course) {
    return course < 32 && p.ttBestMs[course] != 0 && p.ttBestMs[course] <= unlock_tables::ExpertGhostMs[course];
}
unsigned expertGhostCount(const Progress& p) {
    unsigned n = 0;
    for (unsigned c = 0; c < 32; ++c) n += expertGhostAppeared(p, c);
    return n;
}
unsigned timeTrialCourses(const Progress& p) {
    unsigned n = 0;
    for (auto t : p.ttBestMs) n += t != 0;
    return n;
}

bool conditionMet(const Progress& p, unsigned id) {
    if (id < 16) {
        // Star (cup 2), Special (3), Leaf (6), Lightning (7) per class; condition id % 4 = class.
        const unsigned cls = id % 4, group = id / 4;
        constexpr unsigned cup[4] = {2, 3, 6, 7};
        if (cupUnlocked(p.flags, cup[group], cls, false)) return false;
        switch (group) {
        case 0: return trophy(p, cls, 0) && trophy(p, cls, 1);
        case 1: return trophy(p, cls, 2);
        case 2: return trophy(p, cls, 4) && trophy(p, cls, 5);
        default: return trophy(p, cls, 6);
        }
    }
    const unsigned wfc = p.wfcWins, ghost = p.ghostRaceWins, ee = expertGhostCount(p), tt = timeTrialCourses(p);
    switch (id) {
    case 16: if (!flagClear(p, BitMirror)) return false;
             for (unsigned c = 0; c < 8; ++c) if (!gold(p, 2, c)) return false;
             return true;
    case 17: return charLocked(p, 5) && gold(p, 1, 6);            // Dry Bones: 100cc Leaf
    case 18: return charLocked(p, 13) && tt == 32;                // Toadette: TT on every course
    case 19: return charLocked(p, 13) && wfc >= 1000;
    case 20: return charLocked(p, 4) && cupsStar(p, 0, 0, 4);     // Baby Daisy
    case 21: return charLocked(p, 12) && ee >= 8;                 // Baby Luigi
    case 22: return charLocked(p, 18) && gold(p, 0, 7);           // Diddy Kong: 50cc Lightning
    case 23: return charLocked(p, 20) && cupsStar(p, 1, 4, 4);    // Bowser Jr.
    case 24: return charLocked(p, 15) && gold(p, 2, 3);           // Daisy: 150cc Special
    case 25: return charLocked(p, 17) && tt >= 16;                // Birdo
    case 26: return charLocked(p, 23) && cupsStar(p, 3, 0, 8);    // Rosalina: all Mirror cups
    case 27: return charLocked(p, 23) && p.galaxySave && capped(p.races) + capped(p.battles) > 49;
    case 28: return charLocked(p, 21) && cupsStar(p, 2, 0, 4);    // Dry Bowser
    case 29: return charLocked(p, 22) && ee >= 4;                 // Funky Kong
    case 30: return charLocked(p, 22) && ghost >= 25;
    case 31: return charLocked(p, 19) && gold(p, 0, 2);           // King Boo: 50cc Star
    case 32: return flagClear(p, 12) && gold(p, 1, 3);            // Mii Outfit A: 100cc Special
    case 33: return flagClear(p, 13) && ee >= 32;                 // Mii Outfit B
    case 34: return flagClear(p, 13) && wfc >= 5000;
    case 35: return charLocked(p, 12) && ghost >= 100;
    case 36: return charLocked(p, 17) && wfc >= 250;
    case 37: return vehLocked(p, 9) && cupsStar(p, 0, 4, 4);      // Cheep Charger
    case 38: return vehLocked(p, 12) && ee >= 1;                  // Tiny Titan
    case 39: return vehLocked(p, 12) && wfc >= 50;
    case 40: return vehLocked(p, 15) && gold(p, 3, 7);            // Blue Falcon: Mirror Lightning
    case 41: return vehLocked(p, 10) && gold(p, 0, 6);            // Super Blooper: 50cc Leaf
    case 42: return vehLocked(p, 13) && gold(p, 2, 6);            // Daytripper: 150cc Leaf
    case 43: return vehLocked(p, 16) && ee >= 24;                 // Sprinter
    case 44: return vehLocked(p, 11) && gold(p, 0, 3);            // Piranha Prowler: 50cc Special
    case 45: return vehLocked(p, 14) && cupsStar(p, 2, 4, 4);     // Jetsetter
    case 46: return vehLocked(p, 17) && gold(p, 2, 7);            // Honeycoupe: 150cc Lightning
    case 47: return vehLocked(p, 27) && gold(p, 2, 2);            // Quacker: 150cc Star
    case 48: return vehLocked(p, 30) && tt >= 8;                  // Magikruiser
    case 49: return vehLocked(p, 30) && ghost >= 5;
    case 50: return vehLocked(p, 33) && gold(p, 3, 6);            // Jet Bubble: Mirror Leaf
    case 51: return vehLocked(p, 28) && gold(p, 1, 7);            // Zip Zip: 100cc Lightning
    case 52: return vehLocked(p, 31) && cupsStar(p, 1, 0, 4);     // Sneakster
    case 53: return vehLocked(p, 34) && gold(p, 3, 2);            // Dolphin Dasher: Mirror STAR (licence +0xBD6)
    case 54: return vehLocked(p, 29) && gold(p, 1, 2);            // Shooting Star: 100cc Star
    case 55: return vehLocked(p, 32) && ee >= 12;                 // Spear
    case 56: return vehLocked(p, 32) && ghost >= 200;
    case 57: return vehLocked(p, 35) && gold(p, 3, 3);            // Phantom: Mirror Special
    case 58: return flagClear(p, BitMixed50) && trophy(p, 0, 3) && trophy(p, 0, 7);
    case 59: return flagClear(p, BitMixed100) && trophy(p, 1, 3) && trophy(p, 1, 7);
    case 60: return vehLocked(p, 16) && wfc >= 3000;
    default: return false;
    }
}

unsigned conditionBit(unsigned id) {
    constexpr std::uint8_t bits[ConditionCount] = {
        14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 52,
        0, 1, 1, 2, 3, 4, 5, 6, 7, 11, 11, 8, 10, 10, 9, 12, 13, 13, 3, 7,
        34, 35, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 44, 45, 46, 47, 48, 49, 50, 50, 51, 30, 31, 39};
    return id < ConditionCount ? bits[id] : NoBit;
}

int raceRouteThreshold(unsigned bit) {
    constexpr int characters[14] = {1050, 2550, 1950, 3150, 450, 3450, 2850, 1350, 4350, 750, 2250, 4950, 1650, 5100};
    constexpr int vehicles[19] = {1800, 1200, 4200, 300, 2700, 4650, 600, 4500, 3000, 2400, 900, 3900, 2100, 3300, 3750, 1500, 3600, 4050, 4800};
    if (bit < 14) return characters[bit];
    if (bit >= 34 && bit <= 52) return vehicles[bit - 34];   // 52 = Mirror
    return -1;
}
bool raceRouteMet(const Progress& p, unsigned bit) {
    if (flag(p.flags, bit)) return false;
    const int need = raceRouteThreshold(bit);
    return need >= 0 && unsigned(need) <= capped(p.races) + capped(p.battles);
}

std::optional<Section> nextSection(const Progress& p) {
    const int id = firstCondition(p);
    if (id >= 0) return sectionOfCondition(unsigned(id));
    const int bit = firstRaceBit(p);
    return bit >= 0 ? sectionOfRaceBit(unsigned(bit)) : std::nullopt;
}

std::optional<Notice> activateNext(Progress& p) {
    Notice n;
    const int id = firstCondition(p);
    if (id >= 0) {
        const unsigned c = unsigned(id);
        n.condition = id; n.bit = conditionBit(c); n.section = sectionOfCondition(c);
        setFlag(p.flags, n.bit);
        // fn_1_3443A0: the condition text.
        if (c < 16) { n.conditionText = std::uint16_t(0x1FA4 + c / 4); n.conditionArg = className(c % 4); }
        else if (c == 16) n.conditionText = 0x1FA8;
        else if (c <= 36) n.conditionText = std::uint16_t(0x1F9D + c);
        else n.conditionText = std::uint16_t(0x1FA7 + c);
        // fn_1_34445C: the headline.
        const int character = characterOfBit(n.bit), vehicle = vehicleOfBit(n.bit);
        if (c == 16) { n.headline = 0x1F41; n.cupIcon = 8; }
        else if (c < 16) {
            constexpr unsigned cup[4] = {2, 3, 6, 7};
            n.headline = 0x1F40; n.headlineArg[0] = className(c % 4); n.headlineArg[1] = cupName(cup[c / 4]); n.cupIcon = int(cup[c / 4]);
        }
        else if (c == 58) n.headline = 0x1F4A;
        else if (c == 59) n.headline = 0x1F4B;
        else personHeadline(n, character, vehicle);
        models(n, character, vehicle);
        return n;
    }
    const int bit = firstRaceBit(p);
    if (bit < 0) return std::nullopt;   // retail would fall through to bit 0x3A; the flow never activates the page then
    n.bit = unsigned(bit); n.section = *sectionOfRaceBit(n.bit);
    setFlag(p.flags, n.bit);
    n.conditionText = 0x1FE4; n.conditionNumber = std::uint32_t(raceRouteThreshold(n.bit));
    const int character = characterOfBit(n.bit), vehicle = vehicleOfBit(n.bit);
    if (n.bit == BitMirror) { n.headline = 0x1F41; n.cupIcon = 8; }
    else personHeadline(n, character, vehicle);
    models(n, character, vehicle);
    return n;
}

unsigned nextDelayFrames(const Notice& n, unsigned buttonDelay) {
    return n.modelCharacter >= 0 || n.modelVehicle >= 0 ? 90u : buttonDelay;
}
std::uint16_t nextVoice(const Notice& n) {
    if (n.modelVehicle >= 0 || n.modelCharacter < 0 || isMii(unsigned(n.modelCharacter))) return 0;
    const unsigned c = unsigned(n.modelCharacter);
    if (c >= unlock_tables::UnlockVoice.size() || unlock_tables::UnlockVoice[c] == 0x392) return 0;
    return unlock_tables::UnlockVoice[c];
}

Credits creditsAfterAward(const Progress& p, unsigned cup, unsigned cls, bool mirror, unsigned place) {
    if (place == 0 || place > 3) return Credits::None;
    if (mirror) cls = 3;
    cls &= 3;
    const bool specialOrLightning = cup == 3 || cup == 7;
    if (place == 1) {
        if (!flag(p.flags, BitTrueEnding)) {
            bool allGold = true;
            for (unsigned k = 0; k < 4 && allGold; ++k)
                for (unsigned c = 0; c < 8; ++c)
                    if (!gold(p, k, c)) { allGold = false; break; }
            if (allGold) return Credits::True;
        } else if (specialOrLightning) return Credits::True;
    }
    if (specialOrLightning && trophy(p, cls, 3) && trophy(p, cls, 7)) return Credits::Normal;
    return Credits::None;
}
AfterAwardResult afterAward(const Progress& p, unsigned cup, unsigned cls, bool mirror, unsigned place) {
    switch (creditsAfterAward(p, cup, cls, mirror, place)) {
    case Credits::Normal: return {AfterAward::CreditsNormal};
    case Credits::True: return {AfterAward::CreditsTrue};
    default: break;
    }
    if (const auto s = nextSection(p)) return {AfterAward::UnlockPages, *s};
    return {AfterAward::MainMenu};
}
bool creditsSkippable(const UnlockWords& f, Credits k) {
    return k == Credits::Normal ? flag(f, BitNormalEnding) : k == Credits::True ? flag(f, BitTrueEnding) : false;
}
bool markEndingSeen(UnlockWords& f, Credits k) {
    const unsigned bit = k == Credits::Normal ? BitNormalEnding : k == Credits::True ? BitTrueEnding : NoBit;
    if (bit == NoBit || flag(f, bit)) return false;
    setFlag(f, bit);
    return true;
}

int CharacterGrid::character(unsigned slot) const {
    if (slot < rows * 6) return button[slot];
    if (slot < count()) return mii[slot - rows * 6];
    return -1;
}
CharacterGrid characterGrid(const UnlockWords& f) {
    CharacterGrid g;
    bool open[3][8];
    unsigned largest = 0;
    for (unsigned c = 0; c < 3; ++c) {
        unsigned n = 0;
        for (unsigned i = 0; i < 8; ++i) n += open[c][i] = characterUnlocked(f, unlock_tables::CharacterSelectOrder[c][i]);
        largest = std::max(largest, n);
    }
    const bool miiA = characterUnlocked(f, 0x18), miiB = characterUnlocked(f, 0x1A);
    g.miiButtons = unsigned(miiA) + unsigned(miiB);
    g.rows = (largest + 1) / 2;
    if (g.miiButtons) g.rows = std::clamp(g.rows, 3u, 4u);
    const unsigned span = g.rows * 2;
    g.button.fill(-1);
    for (unsigned c = 0; c < 3; ++c)
        for (unsigned k = 0; k < span; ++k) {
            int id = -1;
            if (open[c][k]) id = unlock_tables::CharacterSelectOrder[c][k];
            else {
                // A locked home slot takes the next unlocked character whose home
                // slot lies outside the grid (getCharacterIdForButton 0x807E3CAC).
                int locked = 0;
                for (unsigned i = 0; i <= k; ++i) locked += !open[c][i];
                for (unsigned i = span; i < 8; ++i) {
                    if (open[c][i]) --locked;
                    if (locked == 0) { id = unlock_tables::CharacterSelectOrder[c][i]; break; }
                }
            }
            g.button[c * span + k] = id;
        }
    for (unsigned j = 0; j < g.miiButtons; ++j)
        g.mii[j] = int(0x1E + 2 * ((j + (miiA ? 0 : 1)) % g.rows));
    return g;
}

VehicleColumns vehicleColumns(const UnlockWords& f, unsigned weight) {
    VehicleColumns v;
    unsigned largest = 0;
    for (unsigned w = 0; w < 3; ++w)
        for (unsigned kind = 0; kind < 2; ++kind) {
            unsigned n = 0;
            for (unsigned i = 0; i < 6; ++i) n += vehicleUnlocked(f, w + 3 * i + 18 * kind);
            largest = std::max(largest, n);
        }
    v.rows = largest >= 3 && largest <= 5 ? largest : 6;
    weight %= 3;
    for (unsigned kind = 0; kind < 2; ++kind) {
        v.column[kind].fill(-1);
        unsigned row = 0;
        for (unsigned i = 0; i < 6; ++i) {
            const unsigned id = weight + 3 * i + 18 * kind;
            if (vehicleUnlocked(f, id)) v.column[kind][row++] = int(id);
        }
    }
    return v;
}
}
