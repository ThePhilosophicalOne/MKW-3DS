#pragma once
// The award scene's effect and sound rules as pure functions (host-tested in
// tools/tests/gp_end.cpp); src/ceremony.cpp (CeremonyEffects) runs them on the live scene.
//
// winningrun_demo places three effect objects at the world origin (KMP GOBJ, identity
// rotation, scale 1); their breff emitters carry world positions (rk_kamifubukiALL0 at
// (2100, 3000, 38000), the firework seeds at (0, 7910, -22600)...), so the effects are set at
// the object's position only. PAL StaticR addresses.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mkw {

// Sound ids (include/mkw/sound_ids.hpp; every one is in GRP_AWARD_DEMO, BRSAR group 8).
constexpr uint16_t SeDemoHanabi = 0x215, SeDemoJugem = 0x216, SeDemoTorophy = 0x21A, SeDemoLoseBirds = 0x21B;

// DemoEf (object 0x2D8; ctor fn_1_1AF448 0x806BF7FC: EGG::Effect "rk_demoHanabi", creator
// 0xE). Calc fn_1_1AF4D4 0x806BF888, every frame once RaceManager +0x20 (the frame counter,
// Kinoko RaceManager::timer) is above 0x527: create (vt+0xC, a new instance unless one is
// running), setTranslate (vt+0x74) to the object's position, update (vt+0x80); then
// HoldSoundLimited(0.0, SE_DEMO_HANABI) on the object's actor (fn_1_310168 0x8082051C).
constexpr uint32_t DemoFireworksAfter = 0x527;
inline bool demoFireworksOn(uint32_t raceTimer) { return raceTimer > DemoFireworksAfter; }

// Kamifubuki (object 0x2E5; ctor fn_1_1AF5A4 0x806BF958: "rk_kamifubuki" +0xB0 and
// "rk_kamifubukiLast" +0xB4, creator 0). Calc fn_1_1AF640 0x806BF9F4 by CameraManager
// +0x28 (the award camera phase, AwardCameraPhase) and +0x25 (DemoStage::cameraCut):
//   phase < 1: on the cut, create rk_kamifubuki; translate and update it every frame;
//   phase >= 1: fade rk_kamifubuki (vt+0x10), create rk_kamifubukiLast, translate, update.
struct ConfettiCalls { bool createMain = false, moveMain = false, fadeMain = false, createLast = false, moveLast = false; };
inline ConfettiCalls demoConfetti(int cameraPhase, bool cameraCut) {
    ConfettiCalls c;
    if (cameraPhase < 1) { c.createMain = cameraCut; c.moveMain = true; }
    else { c.fadeMain = true; c.createLast = c.moveLast = true; }
    return c;
}

// DemoJugemu (object 0x2DB). Load fn_1_24E490 0x8075E844 sets +0xB8 when RaceConfig cameraMode
// is 8..10 (VS / team / battle award). Calc fn_1_24E5D8 0x8075E98C: on CameraManager +0x25 or
// +0xB8 (then cleared), CHR0 "o_jg_demowin" from frame 0 at rate 1 (fn_1_47018),
// StartSoundLimited(0.0, SE_DEMO_JUGEM) (fn_1_310148 0x808204FC) and create "rk_JugemTail";
// every frame the tail goes to the model's "root" joint.
inline bool demoLakituVsFlag(unsigned cameraMode) { return cameraMode >= 8 && cameraMode <= 10; }
inline bool demoLakituStart(bool cameraCut, bool vsFlag) { return cameraCut || vsFlag; }

// The award results trophy (lbl_1_bss_5568 0x809C2C48, made by fn_1_23C1FC 0x8074C5B0 in
// gameMode 0xB; interface vtable 0x808CBCF8). Page 0x3C (Win only: fn_1_ACB7C 0x805BCF30
// clears it for cameraMode 0xC) drives it:
//   onInit  -> +0x08 load  fn_1_23C4F4 0x8074C8A8: "/Demo/Trophy/<cup>_trophy.brres" model
//              "<cup>_trophy" in GfxManager draw group 0x12, CHR0 + CLR0 "<cup>_trophy"
//              (anims 0 and 1), Effect/rk_trophy.breff, the effect by standing (+0x4C);
//   onActivate -> +0x0C init fn_1_23C96C 0x8074CD20: at (700, 0, 0), scale 3, CLR0 frame
//              0 / 5 / 9 (gold / silver / bronze, 0x808A2CFC / 0x808A2D00 / 0x808A2D04), rate 0;
//   beforeInAnim -> +0x14 start fn_1_23CA9C 0x8074CE50: AwardTrophyMotion::start, then
//              SE_DEMO_TOROPHY on the 2D race SE player (fn_1_1EC7EC 0x806FCBA0);
//   afterCalc -> +0x10 calc fn_1_23CB4C 0x8074CF00: AwardTrophyMotion::calc, the effect
//              created, put at the trophy, scaled by 3 and updated.
inline constexpr const char* TrophyCups[9] = {"kinoko", "flower", "star", "special", "koura", "banana", "konoha", "thunder", "battle"};
inline float trophyColourFrame(unsigned standing) { return standing == 1 ? 0.0f : standing == 2 ? 5.0f : 9.0f; }
inline const char* trophyEffect(unsigned standing) {
    return standing == 1 ? "rk_trophyGold" : standing == 2 ? "rk_trophySilver" : standing == 3 ? "rk_trophyCopper" : nullptr;
}
struct AwardTrophyMotion {
    static constexpr float StartX = 700.0f;      // lbl_1_data_190F0 0x808CBCC0 (static Vec3 lbl_1_bss_5578)
    static constexpr float Amplitude = 700.0f;   // 0x808CBCCC
    static constexpr float SpinStart = -30.0f;   // lbl_1_data_190F4 0x808CBCC4, degrees a frame
    static constexpr float SpinAmplitude = -29.8f; // 0x808CBCC8
    static constexpr float PhaseStep = 1.8f;     // 0x808CBCD0, degrees a frame
    static constexpr float DegToRad = 0.0174533f; // lbl_1_rodata_135EC 0x808A2D0C
    static constexpr float Scale = 3.0f;         // lbl_1_rodata_135D8 0x808A2CF8
    float x = StartX, y = 0, z = 0;
    float rotation = 3.14159274f;                // +0x3C radians (lbl_1_rodata_135E8 0x808A2D08)
    float spin = SpinStart, phase = 0;
    bool sliding = false;                        // +0x1C state 1
    void start() { sliding = true; x = StartX; y = z = 0; rotation = 3.14159274f; spin = SpinStart; phase = 0; }
    // One afterCalc frame; sinRad = EGG::Mathf::sin (a radian argument).
    template <class Sin> void calc(Sin sinRad) {
        if (sliding) {
            phase += PhaseStep;
            const double s = sinRad(phase * DegToRad);
            x = StartX + float(-double(Amplitude) * s);
            spin = SpinStart - float(double(SpinAmplitude) * s);
            if (x <= 0.0f) { x = 0.0f; sliding = false; }
        }
        rotation += spin * DegToRad;
    }
};

// ---- REFX v3 subset (tools/cook_race_effects.py format, as RaceEffects::loadMemory reads it).
// The emitters reachable from `roots` through their particle curves' child / postfield name
// tables (kinds 2 and 5), in table order, with only the textures they use. Emitter and
// texture indices are renumbered; the payload is kept whole. False if a root is missing or
// the table is malformed. `used` gets the kept textures' old indices (their t3x order).
inline bool refxSubset(const std::vector<uint8_t>& in, const char* const* roots, size_t rootCount,
                       std::vector<uint8_t>& out, std::vector<uint16_t>& used) {
    struct Head { char magic[4]; uint32_t version, emitters, textures, blob; };
    struct Rec { char name[32]; uint32_t desc, ptcl; uint16_t texture[3], ptclTracks, ptclInit, emitTracks, emitInit, ptclSize, pad[2]; uint32_t list; };
    static_assert(sizeof(Head) == 20 && sizeof(Rec) == 64, "REFX v3");
    constexpr size_t TexBytes = 40;
    auto be32 = [](const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; };
    auto be16 = [](const uint8_t* p) { return uint32_t(p[0]) << 8 | p[1]; };
    auto put32 = [](uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v); };
    Head h;
    if (in.size() < sizeof h) return false;
    std::memcpy(&h, in.data(), sizeof h);
    if (std::memcmp(h.magic, "REFX", 4) || h.version != 3 ||
        in.size() != sizeof h + size_t(h.textures) * TexBytes + size_t(h.emitters) * sizeof(Rec) + h.blob) return false;
    const uint8_t* tex = in.data() + sizeof h;
    std::vector<Rec> rec(h.emitters);
    std::memcpy(rec.data(), tex + size_t(h.textures) * TexBytes, rec.size() * sizeof(Rec));
    std::vector<uint8_t> blob(tex + size_t(h.textures) * TexBytes + rec.size() * sizeof(Rec), in.data() + in.size());
    // Each particle curve with a resolved name table: (curve offset, table offset, kind).
    struct Table { uint32_t curve, at; uint8_t kind; };
    std::vector<std::vector<Table>> tables(rec.size());
    for (size_t i = 0; i < rec.size(); ++i) {
        const Rec& r = rec[i];
        const uint32_t curves = uint32_t(r.ptclTracks) + r.emitTracks;
        if (curves && (r.list > h.blob || h.blob - r.list < curves * 4)) return false;
        for (uint32_t k = 0; k < r.ptclTracks; ++k) {
            uint32_t c;
            std::memcpy(&c, blob.data() + r.list + k * 4, 4);
            if (c > h.blob || h.blob - c < 0x40) return false;
            const uint8_t* p = blob.data() + c;
            if (!((p[0] == 0xab || p[0] == 0xac) && (p[2] == 2 || p[2] == 4 || p[2] == 5) && be32(p + 24))) continue;
            const uint32_t at = c + 32 + be32(p + 12) + be32(p + 16) + be32(p + 20);
            if (at > h.blob || h.blob - at < 4 || h.blob - at - 4 < be16(blob.data() + at) * 4) return false;
            tables[i].push_back({c, at, p[2]});
        }
    }
    auto find = [&](const char* name) {
        for (size_t i = 0; i < rec.size(); ++i) if (!std::strncmp(rec[i].name, name, 32)) return int(i);
        return -1;
    };
    std::vector<uint8_t> keep(rec.size(), 0);
    std::vector<size_t> work;
    for (size_t r = 0; r < rootCount; ++r) {
        const int i = find(roots[r]);
        if (i < 0) return false;
        if (!keep[size_t(i)]) { keep[size_t(i)] = 1; work.push_back(size_t(i)); }
    }
    while (!work.empty()) {
        const size_t i = work.back();
        work.pop_back();
        for (const Table& t : tables[i]) {
            if (t.kind == 4) continue;
            for (uint32_t n = 0; n < be16(blob.data() + t.at); ++n) {
                const uint32_t v = be32(blob.data() + t.at + 4 + n * 4);
                if (v == 0xffffffffu) continue;
                if (v >= rec.size()) return false;
                if (!keep[v]) { keep[v] = 1; work.push_back(v); }
            }
        }
    }
    std::vector<uint32_t> emitterMap(rec.size(), 0xffffffffu), textureMap(h.textures, 0xffffu);
    uint32_t emitters = 0;
    used.clear();
    auto useTexture = [&](uint32_t t) {
        if (t >= h.textures) return false;
        if (textureMap[t] == 0xffffu) { textureMap[t] = uint16_t(used.size()); used.push_back(uint16_t(t)); }
        return true;
    };
    for (size_t i = 0; i < rec.size(); ++i) {
        if (!keep[i]) continue;
        emitterMap[i] = emitters++;
        for (uint16_t t : rec[i].texture) if (t != 0xffff && !useTexture(t)) return false;
        for (const Table& tb : tables[i])
            if (tb.kind == 4)
                for (uint32_t n = 0; n < be16(blob.data() + tb.at); ++n)
                    if (const uint32_t v = be32(blob.data() + tb.at + 4 + n * 4); v != 0xffffffffu && !useTexture(v)) return false;
    }
    // Renumber the kept emitters' name tables in the payload copy (each table once).
    std::vector<uint32_t> done;
    std::vector<Rec> kept;
    for (size_t i = 0; i < rec.size(); ++i) {
        if (!keep[i]) continue;
        Rec r = rec[i];
        for (uint16_t& t : r.texture) if (t != 0xffff) t = uint16_t(textureMap[t]);
        for (const Table& tb : tables[i]) {
            bool seen = false;
            for (uint32_t d : done) seen |= d == tb.at;
            if (seen) continue;
            done.push_back(tb.at);
            for (uint32_t n = 0; n < be16(blob.data() + tb.at); ++n) {
                uint8_t* slot = blob.data() + tb.at + 4 + n * 4;
                const uint32_t v = be32(slot);
                if (v != 0xffffffffu) put32(slot, tb.kind == 4 ? textureMap[v] : emitterMap[v]);
            }
        }
        kept.push_back(r);
    }
    Head o = h;
    o.emitters = emitters;
    o.textures = uint32_t(used.size());
    out.assign(sizeof o, 0);
    std::memcpy(out.data(), &o, sizeof o);
    for (uint16_t t : used) out.insert(out.end(), tex + size_t(t) * TexBytes, tex + size_t(t + 1) * TexBytes);
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(kept.data()), reinterpret_cast<const uint8_t*>(kept.data() + kept.size()));
    out.insert(out.end(), blob.begin(), blob.end());
    return true;
}

} // namespace mkw
