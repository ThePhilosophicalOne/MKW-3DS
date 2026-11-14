#include <mkw/demo_stage.hpp>
#include <cmath>
#include <cstring>

namespace mkw {

DemoStageConfig gpAwardStage(GpAwardScene scene, const std::array<uint8_t, GpMaxRacers>& standing,
                             const std::array<uint8_t, GpMaxRacers>& character, const std::array<uint8_t, GpMaxRacers>& vehicle,
                             uint32_t humanMask, unsigned count) {
    DemoStageConfig c;
    count = count < GpMaxRacers ? count : GpMaxRacers;
    c.kind = scene == GpAwardScene::Win ? DemoStageKind::Award : DemoStageKind::Lose;
    c.course = scene == GpAwardScene::Win ? WinningRunCourse : LoserCourse;
    c.gameMode = AwardGameMode;
    c.cameraMode = scene == GpAwardScene::Win ? AwardCameraModeWin : AwardCameraModeLose;
    c.music = scene == GpAwardScene::Win ? 0xCA : 0xCD; // fn_1_201710 0x80711AC4, section 0x35
    c.camera = DemoCamera::Scn0;
    unsigned n = 0;
    // Racedata_initAwards: by standing 1..12; cameraMode 7 takes ranks 1-3 of any type,
    // cameraMode 0xC only the local humans.
    for (unsigned rank = 1; rank <= count && n < GpMaxRacers; ++rank)
        for (unsigned i = 0; i < count; ++i) {
            if (standing[i] != rank) continue;
            const bool human = humanMask >> i & 1;
            if (scene == GpAwardScene::Win ? rank <= 3 : human) {
                auto& r = c.racers[n];
                r.present = true; r.human = human; r.character = character[i]; r.vehicle = vehicle[i];
                r.startPoint = uint8_t(awardStartPoint(n, scene));
                ++n;
            }
            break;
        }
    c.racerCount = n;
    return c;
}

DemoStageConfig endingStage(const std::array<int8_t, GpMaxRacers>& order, const std::array<uint8_t, GpMaxRacers>& character,
                            const std::array<uint8_t, GpMaxRacers>& vehicle, uint32_t humanMask) {
    DemoStageConfig c;
    c.kind = DemoStageKind::Ending;
    c.course = 0x3A;      // ending_demo
    c.gameMode = 0xC;     // credits
    c.cameraMode = 0;
    c.music = 206;        // STRM_O_ENDING_PART_A
    c.camera = DemoCamera::KmpChain;
    c.firstKmpCamera = 0x10;
    unsigned n = 0;
    for (unsigned k = 0; k < GpMaxRacers; ++k) {
        const int i = order[k];
        if (i < 0 || unsigned(i) >= GpMaxRacers) continue;
        auto& r = c.racers[n];
        r.present = true; r.human = humanMask >> i & 1;
        r.character = character[unsigned(i)]; r.vehicle = vehicle[unsigned(i)];
        r.startPoint = uint8_t(n);
        ++n;
    }
    c.racerCount = n;
    return c;
}

void DemoStage::tick() {
    if (!active_) return;
    if (config_.kind == DemoStageKind::Award) {
        // fn_1_9032C, then fn_1_90BEC (time += 1, held at the frame count, then "finished").
        if (scn0_ < 0) {
            if (chainDone_) { scn0_ = 0; phase_ = AwardCameraPhase::Run; cameraTime_ = 0; cameraDone_ = false; kartsVisible_ = false; }
        } else {
            if (cameraDone_ && phase_ == AwardCameraPhase::Run) {
                scn0_ = 1; phase_ = AwardCameraPhase::Podium; cameraTime_ = 0; cameraDone_ = false; kartsVisible_ = false;
            } else if (!cameraDone_) {
                cameraTime_ += 1.0f;
                if (cameraFrames_ && float(cameraFrames_) < cameraTime_) { cameraTime_ = float(cameraFrames_); cameraDone_ = true; }
            }
        }
    }
    // Lose: demolos01 stays at frame 0 (fn_1_90BEC, +0x26 == 0); the credits run uses the
    // host's KMP camera chain.
    ++frame_;
    if (callback_) callback_(context_, frame_);
}

void awardPodiumLayout(const std::array<uint8_t, 3>& podiumWeight, unsigned podiumCount,
                       const std::array<uint8_t, GpMaxRacers>& bystanderWeight, unsigned bystanderCount,
                       std::array<AwardPlacement, 3>& podium, std::array<AwardPlacement, GpMaxRacers>& bystanders) {
    constexpr float Shadow[3] = {0.4f, 0.5f, 0.75f};       // 0x808A4830
    constexpr float Depth[3] = {100.0f, 100.0f, 250.0f};   // 0x808A4880
    constexpr float HalfPi = 1.57079637f;                  // 0x808A492C
    podium = {}; bystanders = {};
    const float ox = AwardOrigin[0], oy = AwardOrigin[1], oz = AwardOrigin[2];
    for (unsigned i = 0; i < podiumCount && i < 3; ++i) {
        auto& p = podium[i];
        p.used = true; p.x = ox; p.y = oy; p.z = oz;
        if (i == 0) p.y += 140.0f;
        else { p.y += 70.0f; p.x += i == 1 ? -225.0f : 225.0f; }
        p.shadowScale = Shadow[podiumWeight[i] < 3 ? podiumWeight[i] : 0];
    }
    float side[2][3] = {{ox + 200.0f, oy, oz + 145.0f}, {ox - 200.0f, oy, oz + 145.0f}};
    for (unsigned i = 0; i < bystanderCount && i < GpMaxRacers; ++i) {
        const unsigned odd = i & 1, w = bystanderWeight[i] < 3 ? bystanderWeight[i] : 0;
        float* s = side[odd];
        s[2] += Depth[w];
        auto& p = bystanders[i];
        p.used = true; p.x = s[0]; p.y = s[1]; p.z = s[2];
        if (i >> 1 & 1) p.x += odd ? -700.0f : 700.0f;
        p.yaw = odd ? HalfPi : -HalfPi;
        p.shadowScale = Shadow[w];
        s[2] += Depth[w];
    }
}

bool ScamTrack::load(const void* data, size_t size) {
    values_ = nullptr; frames_ = 0;
    const auto* b = static_cast<const uint8_t*>(data);
    if (!b || size < 12 || std::memcmp(b, "SCAM", 4) != 0) return false;
    auto u32 = [&](size_t o) { return uint32_t(b[o]) | uint32_t(b[o + 1]) << 8 | uint32_t(b[o + 2]) << 16 | uint32_t(b[o + 3]) << 24; };
    const uint32_t version = u32(4), frames = u32(8);
    if (version != 1 || frames == 0 || size < 12 + size_t(frames) * 13 * sizeof(float)) return false;
    if (reinterpret_cast<uintptr_t>(b + 12) % alignof(float)) return false; // the loader keeps 4-byte alignment
    values_ = reinterpret_cast<const float*>(b + 12); // little-endian floats, as the 3DS
    frames_ = frames;
    return true;
}

DemoView scamView(const ScamFrame& f) {
    constexpr float Rad = 3.14159265358979323846f / 180.f;
    const float sx = std::sin(f.rotation[0] * Rad), cx = std::cos(f.rotation[0] * Rad);
    const float sy = std::sin(f.rotation[1] * Rad), cy = std::cos(f.rotation[1] * Rad);
    const float sz = std::sin(f.rotation[2] * Rad), cz = std::cos(f.rotation[2] * Rad);
    // World rotation Ry * Rx * Rz: -Z is the view direction, +Y the up axis.
    const Vec3 forward{-cx * sy, sx, -cx * cy};
    DemoView v;
    v.eye = {f.position[0], f.position[1], f.position[2]};
    v.target = v.eye + forward * 1000.f;
    v.up = {cz * sx * sy - sz * cy, cz * cx, sz * sy + cz * sx * cy};
    v.fov = f.fovy;
    return v;
}

ScamFrame ScamTrack::sample(unsigned frame) const {
    ScamFrame f{};
    if (!values_) return f;
    const float* v = values_ + size_t(frame < frames_ ? frame : frames_ - 1) * 13;
    for (int k = 0; k < 3; ++k) { f.position[k] = v[k]; f.rotation[k] = v[3 + k]; f.aim[k] = v[6 + k]; }
    f.twist = v[9]; f.fovy = v[10]; f.nearZ = v[11]; f.farZ = v[12];
    return f;
}

} // namespace mkw
