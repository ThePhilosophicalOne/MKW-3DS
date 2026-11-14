#include <mkw/ceremony.hpp>
#include <mkw/animation.hpp>
#include <mkw/asset_pack.hpp>
#include <mkw/award_layout.hpp>
#include <mkw/ceremony_fx.hpp>
#include <mkw/course_effects.hpp>
#include <mkw/gp_award.hpp>
#include <mkw/kart_drift_effects.hpp>
#include <mkw/race_effects.hpp>
#include <mkw/renderer.hpp>
// Kinoko's ObjectBreakable::enableCollision hides ObjectBase's const one (as in course_effects.cpp).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#include <game/field/ObjectDirector.hh>
#pragma GCC diagnostic pop
#include <game/kart/KartMove.hh>
#include <game/kart/KartObjectManager.hh>
#include <game/system/CourseMap.hh>
#include <game/system/KPadDirector.hh>
#include <game/system/RaceConfig.hh>
#include <game/system/RaceManager.hh>
#include <game/system/ResourceManager.hh>
#include <game/system/map/MapdataStageInfo.hh>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

namespace mkw {
namespace K = Kinoko;

// ---- SCN0 cameras -------------------------------------------------------------------------

bool ScamFile::load(const char* path) {
    reset();
    long size = pack::size(path);
    if (size > 0) {
        data_.resize((size_t(size) + 3) / 4);
        if (!pack::readInto(path, data_.data(), size_t(size))) data_.clear();
    }
    if (data_.empty()) {
        FILE* f = std::fopen(path, "rb");
        if (!f) return false;
        size = std::fseek(f, 0, SEEK_END) == 0 ? std::ftell(f) : -1;
        bool ok = size >= 12 && size <= 4 * 1024 * 1024 && std::fseek(f, 0, SEEK_SET) == 0;
        if (ok) {
            data_.resize((size_t(size) + 3) / 4);
            ok = std::fread(data_.data(), 1, size_t(size), f) == size_t(size);
        }
        std::fclose(f);
        if (!ok) { data_.clear(); return false; }
    }
    if (!track_.load(data_.data(), size_t(size))) { reset(); return false; }
    return true;
}

// ---- Demo AI pads -------------------------------------------------------------------------

void DemoDriver::reset(unsigned racerCount, unsigned engineClass) {
    count_ = std::min(racerCount, CpuAi::Capacity);
    size_t size = 0;
    const void* kmp = K::System::ResourceManager::Instance()->getFile("course.kmp", &size, K::System::ArchiveId::Course);
    route_.clear();
    route_.load(kmp, size);
    CpuAi::Setup setup;
    setup.route = &route_;
    setup.count = count_;
    setup.engineClass = std::min(engineClass, 2u);
    setup.cpuMode = setup.engineClass;
    setup.demo = true;
    setup.humanMask = 0;
    if (const auto* stage = K::System::CourseMap::Instance()->getStageInfo()) setup.polePosition = stage->polePosition();
    setup.startAtPointZero = false;
    setup.seed = 0x9e3779b9u ^ (unsigned(K::System::RaceConfig::Instance()->raceScenario().course) * 2654435761u);
    ai_.reset(setup);
}

void DemoDriver::beforeStep() {
    auto* objects = K::Kart::KartObjectManager::Instance();
    auto* manager = K::System::RaceManager::Instance();
    if (!objects || !manager) return;
    const bool racing = manager->isStageReached(K::System::RaceManager::Stage::Race);
    const uint32_t mask = count_ >= 32 ? ~0u : (1u << count_) - 1u;
    std::array<unsigned, CpuAi::Capacity> ranks{}, laps{};
    std::array<bool, CpuAi::Capacity> finished{};
    for (unsigned i = 0; i < CpuAi::Capacity; ++i) { ranks[i] = i + 1; laps[i] = 1; }
    ai_.step(mask, ranks, laps, finished, 3);
    for (unsigned i = 0; i < count_; ++i) {
        const auto& c = ai_.command(i);
        auto* pad = K::System::KPadDirector::Instance()->hostController(i);
        // setStickXUnmirrored(7 + (int)(7 * stick)), as RaceGrid::beforeStep.
        const auto trick = racing ? K::System::Trick(std::min<uint8_t>(c.trick, 4)) : K::System::Trick::None;
        const int raw = std::clamp(7 + int(c.stick * 7.f), 0, 14);
        pad->setInputsRawStick(c.buttons, unsigned(raw), 7, trick);
        // KartObjectProxy::isCpu: the kart model and bike lean read the AI's sticks.
        objects->object(i)->move()->setCpuSteer(ai_.ready(), c.animationStick, c.lean);
    }
}

// ---- Award podium -------------------------------------------------------------------------

struct AwardPodium::Impl {
    struct Character {
        std::unique_ptr<Model> model;
        std::unique_ptr<DriverAnimation> bank;
        int clip = -1;
        float start = 0;
        AwardPlacement at;
    };
    std::array<Character, 3 + GpMaxRacers> characters;
    unsigned count = 0;
    std::unique_ptr<Model> stand, shadow;
};

AwardPodium::AwardPodium() : impl_(std::make_unique<Impl>()) {}
AwardPodium::~AwardPodium() = default;
void AwardPodium::reset() { *impl_ = Impl{}; }
bool AwardPodium::loaded() const { return impl_->stand != nullptr; }

unsigned AwardPodium::bytes() const {
    const auto& d = *impl_;
    unsigned n = 0;
    auto model = [&](const Model* m) {
        if (!m) return;
        n += m->vertexCount * unsigned(sizeof(Vertex)) + m->indexCount * unsigned(sizeof(uint16_t));
        for (const auto& t : m->textures) if (t) n += t->bytes();
    };
    model(d.stand.get());
    model(d.shadow.get());
    for (unsigned i = 0; i < d.count; ++i) model(d.characters[i].model.get());
    return n;
}

bool AwardPodium::load(const std::array<Member, 3>& podium, const std::array<Member, GpMaxRacers>& bystanders, unsigned bystanderCount) {
    reset();
    auto& d = *impl_;
    auto model = [](const char* path) {
        auto m = std::make_unique<Model>();
        if (!m->load(path)) m.reset();
        return m;
    };
    // GP: v_stand1 (variant 0; v_stand2-4 are the team variants).
    d.stand = model("models/award_v_stand1.gmdl");
    d.shadow = model("models/award_shadow.gmdl");
    bool ok = d.stand && d.shadow;
    std::array<uint8_t, 3> podiumWeight{};
    std::array<uint8_t, GpMaxRacers> bystanderWeight{};
    unsigned podiumCount = 0;
    while (podiumCount < 3 && podium[podiumCount].used) ++podiumCount;
    bystanderCount = std::min(bystanderCount, GpMaxRacers);
    // getCharacterWeightClass: a racer's vehicle id modulo 3 (small, medium, large).
    for (unsigned i = 0; i < podiumCount; ++i) podiumWeight[i] = uint8_t(podium[i].vehicle % 3);
    for (unsigned i = 0; i < bystanderCount; ++i) bystanderWeight[i] = uint8_t(bystanders[i].vehicle % 3);
    std::array<AwardPlacement, 3> podiumAt;
    std::array<AwardPlacement, GpMaxRacers> bystanderAt;
    awardPodiumLayout(podiumWeight, podiumCount, bystanderWeight, bystanderCount, podiumAt, bystanderAt);
    auto& random = K::System::RaceManager::Instance()->random();
    auto add = [&](const Member& m, const AwardPlacement& at, bool winner) {
        const char* code = gpAwardCharacterCode(m.character);
        if (!code) return false; // Miis: no award body in this port
        char name[32];
        std::snprintf(name, sizeof name, "%s%s", code, gpAwardBikeOutfit(m.character, m.vehicle) ? "3" : "");
        char path[64];
        auto& c = d.characters[d.count];
        std::snprintf(path, sizeof path, "models/award_%s.gmdl", name);
        c.model = model(path);
        if (!c.model) return false;
        c.bank = std::make_unique<DriverAnimation>();
        std::snprintf(path, sizeof path, "models/award_%s.dskm", name);
        if (!c.bank->load(path, *c.model)) { c.model.reset(); c.bank.reset(); return false; }
        c.bank->prepareLitSkinning(*c.model);
        // demowin (ids 0-2) on the podium, demooth (3-5) for the others.
        c.clip = c.bank->find(winner ? "demowin" : "demooth");
        if (c.clip < 0) { c.model.reset(); c.bank.reset(); return false; }
        c.start = random.getF32(c.bank->duration(unsigned(c.clip)));
        c.at = at;
        ++d.count;
        return true;
    };
    for (unsigned i = 0; i < podiumCount; ++i) ok &= add(podium[i], podiumAt[i], true);
    for (unsigned i = 0; i < bystanderCount; ++i) ok &= add(bystanders[i], bystanderAt[i], false);
    return ok;
}

void AwardPodium::draw(Renderer& renderer, unsigned tick) {
    auto& d = *impl_;
    if (!d.stand) return;
    C3D_Mtx stand;
    Mtx_Identity(&stand);
    Mtx_Translate(&stand, AwardOrigin[0], AwardOrigin[1], AwardOrigin[2], true);
    renderer.draw(*d.stand, stand);
    std::array<Renderer::ShadowVolume, 3 + GpMaxRacers> shadows{};
    unsigned shadowCount = 0;
    for (unsigned i = 0; i < d.count; ++i) {
        const auto& c = d.characters[i];
        if (!d.shadow || !c.at.used) continue;
        auto& s = shadows[shadowCount++];
        s.model = d.shadow.get();
        Mtx_Identity(&s.transform);
        Mtx_Translate(&s.transform, c.at.x, c.at.y, c.at.z, true);
        Mtx_Scale(&s.transform, c.at.shadowScale, c.at.shadowScale, c.at.shadowScale);
    }
    renderer.drawShadowVolumes(shadows.data(), shadowCount, .5f);
    for (unsigned i = 0; i < d.count; ++i) {
        auto& c = d.characters[i];
        if (!c.model || !c.bank || c.clip < 0) continue;
        const float length = c.bank->duration(unsigned(c.clip));
        float frame = c.start + float(tick);
        if (length > 0) frame = c.bank->loop(unsigned(c.clip)) ? std::fmod(frame, length) : std::min(frame, length - 1.f);
        C3D_Mtx m;
        Mtx_Identity(&m);
        Mtx_Translate(&m, c.at.x, c.at.y, c.at.z, true);
        Mtx_RotateY(&m, c.at.yaw, true);
        if (!c.bank->apply(*c.model, c.clip, frame, tick)) continue;
        if (c.bank->gpuSkinned()) renderer.setSkinPalette(c.bank->palette(), c.bank->matrixCount(), 21);
        renderer.draw(*c.model, m);
        renderer.setSkinPalette(nullptr, 0, 12);
    }
}

// ---- Award scene object effects -----------------------------------------------------------

namespace {
// An asset from race.pak, else the loose SD file (the REFX tables and their index are loose).
bool readAsset(const char* path, std::vector<uint8_t>& out, size_t limit) {
    if (pack::read(path, out)) return out.size() <= limit;
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    const long size = std::fseek(f, 0, SEEK_END) == 0 ? std::ftell(f) : -1;
    bool ok = size > 0 && size_t(size) <= limit && std::fseek(f, 0, SEEK_SET) == 0;
    if (ok) { out.resize(size_t(size)); ok = std::fread(out.data(), 1, out.size(), f) == out.size(); }
    std::fclose(f);
    return ok;
}
// The full texture names of one cooked table, in table order (models/effect-archives.json).
bool tableTextures(const std::string& json, const char* table, std::vector<std::string>& out) {
    out.clear();
    size_t at = json.find(std::string("\"") + table + '"');
    if (at == std::string::npos || (at = json.find("\"textures\"", at)) == std::string::npos || (at = json.find('[', at)) == std::string::npos) return false;
    const size_t end = json.find(']', at);
    if (end == std::string::npos) return false;
    for (size_t q = json.find('"', at); q < end; q = json.find('"', q)) {
        const size_t close = json.find('"', q + 1);
        if (close == std::string::npos || close > end) return false;
        out.emplace_back(json, q + 1, close - q - 1);
        q = close + 1;
    }
    return true;
}
// The award objects' EGG::Effect creators: DemoEf 0xE, Kamifubuki 0 (the port's group 0 is
// RaceEffects' slots, so the confetti takes group 1 like CourseEffects).
constexpr uint8_t HanabiGroup = 0xE, ConfettiGroup = 1;
const K::Field::ObjectBase* sceneObject(uint16_t id) {
    if (const auto* director = K::Field::ObjectDirector::Instance())
        for (const auto* o : director->objects())
            if (o && static_cast<uint16_t>(o->id()) == id) return o;
    return nullptr;
}
bool geoPosition(uint16_t id, Vec3& p) {
    if (const auto* map = K::System::CourseMap::Instance())
        for (uint16_t i = 0; i < map->getGeoObjCount(); ++i)
            if (const auto* g = map->getGeoObj(i); g && g->id() == id && (g->presenceFlag() & 1)) {
                p = {g->pos().x, g->pos().y, g->pos().z};
                return true;
            }
    return false;
}
} // namespace

bool CeremonyEffects::load(RaceEffects& effects, Texture* textures, unsigned capacity) {
    unload(effects, textures);
    auto fail = [&](const char* why) { std::snprintf(summary_, sizeof summary_, "ceremony fx: %s", why); return false; };
    if (!effects.ready) return fail("race effects not loaded");
    // The race table as loadRaceVfx built it (race-effects.bin and KartDriftEffects' emitters);
    // after CourseEffects::load on a demo stage (no archive of its own) it is the live one.
    std::vector<uint8_t> table, archive;
    if (!readAsset("textures/race-effects.bin", table, 2u << 20) || !readAsset("textures/fx/common_rkrace/common_rkrace_1.refx", archive, 2u << 20))
        return fail("race table unreadable");
    appendRefxEmitters(table, archive, KartDriftEffects::Names, KartDriftEffects::FxCount);
    uint32_t baseEmitters = 0;
    std::memcpy(&baseEmitters, table.data() + 8, 4);
    std::memcpy(&baseTextures_, table.data() + 12, 4);
    if (baseEmitters != effects.system().resources.size() || baseTextures_ != effects.textures.size()) return fail("race table differs from the live one");
    std::string index;
    if (std::vector<uint8_t> json; readAsset("models/effect-archives.json", json, 4u << 20)) index.assign(json.begin(), json.end());
    // The emitters each object's calc creates, closed under their child references:
    // rk_demoHanabi (13 emitters), rk_kamifubuki and rk_kamifubukiLast (14), rk_JugemTail (1).
    struct Use { const char* key; std::array<const char*, 2> roots; size_t count; };
    constexpr Use Uses[] = {{"winningrun_demo_rk_demo", {"rk_demoHanabi", nullptr}, 1},
                            {"winningrun_demo_rk_kamifubuki", {"rk_kamifubuki", "rk_kamifubukiLast"}, 2},
                            {"winningrun_demo_rk_demojugemu", {"rk_JugemTail", nullptr}, 1}};
    std::vector<std::string> paths, names;
    std::vector<uint8_t> merged = table;
    for (const auto& u : Uses) {
        char path[128];
        std::snprintf(path, sizeof path, "textures/fx/%s/%s_0.refx", u.key, u.key);
        std::vector<uint8_t> full, subset;
        std::vector<uint16_t> used;
        if (!readAsset(path, full, 1u << 20) || !tableTextures(index, path, names) || !refxSubset(full, u.roots.data(), u.count, subset, used))
            return fail("award archive unreadable");
        if (!appendRefxTable(merged, subset)) return fail("race table full");
        for (uint16_t t : used) {
            if (t >= names.size()) return fail("award texture list");
            paths.push_back(std::string("textures/fx/") + u.key + '/' + names[t] + ".t3x");
        }
    }
    if (baseTextures_ + paths.size() > capacity) return fail("texture array full");
    if (!effects.loadMemory(merged.data(), merged.size())) {
        effects.loadMemory(table.data(), table.size());
        return fail("race table reload failed");
    }
    unsigned missing = 0;
    for (const auto& p : paths) {
        Texture& t = textures[baseTextures_ + textureCount_++];
        missing += !t.load(p.c_str());
        bytes_ += t.bytes();
    }
    hanabi_ = effects.find("rk_demoHanabi");
    kamifubuki_ = effects.find("rk_kamifubuki");
    kamifubukiLast_ = effects.find("rk_kamifubukiLast");
    jugemTail_ = effects.find("rk_JugemTail");
    loaded_ = true;
    bytes_ += unsigned(merged.size() - table.size());
    std::snprintf(summary_, sizeof summary_, "ceremony fx: %u emitters, %u textures (%u missing), %u bytes",
                  unsigned(effects.system().resources.size() - baseEmitters), textureCount_, missing, bytes_);
    return missing == 0 && hanabi_ >= 0 && kamifubuki_ >= 0 && kamifubukiLast_ >= 0 && jugemTail_ >= 0;
}

void CeremonyEffects::unload(RaceEffects& effects, Texture* textures) {
    if (!loaded_ && !textureCount_) return;
    effects.reset();
    if (loaded_) {
        std::vector<uint8_t> table, archive;
        if (readAsset("textures/race-effects.bin", table, 2u << 20) && readAsset("textures/fx/common_rkrace/common_rkrace_1.refx", archive, 2u << 20)) {
            appendRefxEmitters(table, archive, KartDriftEffects::Names, KartDriftEffects::FxCount);
            effects.loadMemory(table.data(), table.size());
        }
    }
    for (unsigned i = 0; i < textureCount_; ++i) textures[baseTextures_ + i].reset();
    *this = CeremonyEffects{};
}

bool CeremonyEffects::step(RaceEffects& effects, const DemoStage& stage, const Vec3* lakituRoot) {
    if (!loaded_ || !effects.ready) return false;
    auto& s = effects.system();
    auto alive = [&](const Handle& h) {
        auto* e = static_cast<ef::Effect*>(h.effect);
        return s.alive(e, h.serial) && e->group == h.group;
    };
    // EGG::Effect create (vt+0xC 0x80222D90): a new instance unless one is running. The port's
    // serials stay under 0x8000 in RaceEffects' slots (see CourseEffects).
    auto create = [&](Handle& h, int spec, uint8_t group) {
        if (spec < 0 || alive(h)) return;
        h = {};
        ef::Effect* e = s.createEffect(s.resources[size_t(spec)], 0, group);
        if (!e) return;
        e->serial = uint16_t(0x8000u | (++serial_ & 0x7fffu));
        h = {e, e->serial, group};
    };
    // setTranslate (vt+0x74) and update (vt+0x80): identity rotation, scale 1, at p.
    auto move = [&](const Handle& h, Vec3 p) {
        if (alive(h)) s.setRootMtx(*static_cast<ef::Effect*>(h.effect), {{{1, 0, 0, p.x}, {0, 1, 0, p.y}, {0, 0, 1, p.z}}});
    };
    // Fade (vt+0x10): emitters stop, particles live on, the handle is released.
    auto fade = [&](Handle& h) {
        if (alive(h)) s.retireEmitters(*static_cast<ef::Effect*>(h.effect));
        h = {};
    };
    const auto* race = K::System::RaceManager::Instance();
    using Call = K::Field::ObjectBase::SoundCall;
    Vec3 p;
    // DemoEf (fn_1_1AF4D4 0x806BF888).
    if (race && demoFireworksOn(race->timer()) && geoPosition(0x2D8, p)) {
        create(hanabiFx_, hanabi_, HanabiGroup);
        move(hanabiFx_, p);
        if (const auto* o = sceneObject(0x2D8)) o->cueSound(SeDemoHanabi, Call::Hold, 0.0f);
    }
    // Kamifubuki (fn_1_1AF640 0x806BF9F4).
    if (geoPosition(0x2E5, p)) {
        const ConfettiCalls c = demoConfetti(int(stage.phase()), stage.cameraCut());
        if (c.createMain) create(mainFx_, kamifubuki_, ConfettiGroup);
        if (c.moveMain) move(mainFx_, p);
        if (c.fadeMain) fade(mainFx_);
        if (c.createLast) create(lastFx_, kamifubukiLast_, ConfettiGroup);
        if (c.moveLast) move(lastFx_, p);
    }
    // DemoJugemu (fn_1_24E5D8 0x8075E98C): on the cut the clip restarts (the caller's
    // CourseObjects::startDemoLakitu), SE_DEMO_JUGEM and rk_JugemTail; then every frame the
    // tail goes to the "root" joint's world position as the model's last update left it
    // (fn_80071DC0, translation only: fn_1_277CC).
    bool started = false;
    const bool vsFlag = !lakituFlagUsed_ && demoLakituVsFlag(stage.config().cameraMode);
    if (demoLakituStart(stage.cameraCut(), vsFlag)) {
        lakituFlagUsed_ = started = true;
        if (const auto* o = sceneObject(0x2DB)) o->cueSound(SeDemoJugem, Call::Start, 0.0f);
        create(tailFx_, jugemTail_, ConfettiGroup);
    }
    if (lakituRoot) move(tailFx_, *lakituRoot);
    return started;
}

// ---- Award page textures ------------------------------------------------------------------

namespace {
GPU_TEXTURE_WRAP_PARAM wrapMode(uint8_t gx) { return gx == 1 ? GPU_REPEAT : gx == 2 ? GPU_MIRRORED_REPEAT : GPU_CLAMP_TO_EDGE; }
static_assert(sizeof(FlowQuad) == sizeof(Renderer::MaterialQuad) && std::is_standard_layout_v<FlowQuad> && std::is_standard_layout_v<Renderer::MaterialQuad>,
              "FlowQuad mirrors Renderer::MaterialQuad");
}

struct AwardUi::Impl {
    std::array<Texture, AwardTextures.size()> textures;
    bool loaded = false;
    Renderer* renderer = nullptr;
    static void submit(const FlowBatch& batch, void* context) {
        auto& m = *static_cast<Impl*>(context);
        const Texture* t = batch.texture >= 0 && unsigned(batch.texture) < m.textures.size() ? &m.textures[unsigned(batch.texture)] : nullptr;
        if (batch.texture >= 0 && (!t || !t->ready)) return;
        m.renderer->drawScreenMaterialQuads(t, reinterpret_cast<const Renderer::MaterialQuad*>(batch.quads), batch.count, batch.color0, batch.color1,
                                            wrapMode(batch.wrapS), wrapMode(batch.wrapT));
    }
};

AwardUi::AwardUi() : impl_(std::make_unique<Impl>()) {}
AwardUi::~AwardUi() = default;
bool AwardUi::loaded() const { return impl_->loaded; }

bool AwardUi::load() {
    auto& m = *impl_;
    if (m.loaded) return true;
    for (unsigned i = 0; i < m.textures.size(); ++i)
        if (!m.textures[i].ready && !m.textures[i].load(AwardTextures[i])) { unload(); return false; }
    m.loaded = true;
    return true;
}

void AwardUi::unload() {
    for (auto& t : impl_->textures) t.reset();
    impl_->loaded = false;
}

unsigned AwardUi::bytes() const {
    unsigned n = 0;
    for (const auto& t : impl_->textures) n += t.bytes();
    return n;
}

void AwardUi::drawFade(Renderer& renderer, AwardPageDrawer& page) {
    auto& m = *impl_;
    if (!m.loaded) return;
    m.renderer = &renderer;
    page.drawFade(&Impl::submit, &m);
}

void AwardUi::drawPage(Renderer& renderer, AwardPageDrawer& page) {
    auto& m = *impl_;
    if (!m.loaded) return;
    m.renderer = &renderer;
    page.draw(&Impl::submit, &m);
}

} // namespace mkw
