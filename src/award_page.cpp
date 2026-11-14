#include <mkw/award_page.hpp>
#include <mkw/award_layout.hpp>
#include <mkw/gp_award.hpp>
#include <mkw/gp_rank.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace mkw {
namespace {
// The race overlays' mapping: 456 layout units span the 240-pixel height (race_flow_draw.cpp).
constexpr float Unit = 240.f / 456.f;
struct Placement { float x, y, sx, sy; };

uint32_t pack(const std::array<float, 4>& c) {
    auto b = [](float v) { return uint32_t(std::clamp(std::lround(v), 0l, 255l)); };
    return (b(c[0]) << 24) | (b(c[1]) << 16) | (b(c[2]) << 8) | b(c[3]);
}
uint32_t withAlpha(uint32_t rgba, float alpha) { return (rgba & 0xffffff00u) | uint32_t(std::lround(std::clamp(alpha, 0.f, 1.f) * float(rgba & 0xff))); }
const FlowGlyph* glyph(const FlowFont& font, char16_t c) {
    for (unsigned i = 0; i < font.glyphCount; ++i) if (font.glyphs[i].code == c) return &font.glyphs[i];
    return nullptr;
}
void place(const FlowPanePose& pane, const Placement& at, float lx, float ly, float& sx, float& sy) {
    const float x = pane.m[0] * lx + pane.m[1] * ly + pane.m[2], y = pane.m[3] * lx + pane.m[4] * ly + pane.m[5];
    sx = 200.f + (at.x + x * at.sx) * Unit; sy = 120.f - (at.y + y * at.sy) * Unit;
}
template <class Batch>
void pictureQuad(Batch& batch, const FlowPane& def, const FlowPanePose& pane, const FlowMaterialPose& material, const Placement& at, const float* uvRect = nullptr) {
    auto* q = batch.add(); if (!q) return;
    const float left = -float(def.origin % 3) * pane.w * .5f, top = float(def.origin / 3) * pane.h * .5f;
    const float lx[4] = {left, left + pane.w, left, left + pane.w}, ly[4] = {top, top, top - pane.h, top - pane.h};
    const auto& srt = material.srt;
    const float r = srt[2] * 3.14159265358979323846f / 180.f, c = std::cos(r), s = std::sin(r);
    for (unsigned k = 0; k < 4; ++k) {
        place(pane, at, lx[k], ly[k], q->x[k], q->y[k]);
        float u = def.uv[k * 2], v = def.uv[k * 2 + 1];
        if (uvRect) { u = uvRect[0] + u * (uvRect[2] - uvRect[0]); v = uvRect[1] + v * (uvRect[3] - uvRect[1]); }
        else { const float du = u - .5f, dv = v - .5f; u = srt[3] * c * du - srt[4] * s * dv + srt[0] + .5f; v = srt[3] * s * du + srt[4] * c * dv + srt[1] + .5f; }
        q->u[k] = u; q->v[k] = v; q->color[k] = withAlpha(pane.vertex[k], pane.alpha);
    }
}
// One NW4R text line (as race_flow_draw.cpp textQuads); text is UTF-16 code units.
template <class Batch>
void textQuads(Batch& batch, const FlowPane& def, const FlowPanePose& pane, const Placement& at, const char16_t* text, unsigned length, const FlowFont& font) {
    if (!text || !length) return;
    const float scaleX = def.fontW / float(font.width), scaleY = def.fontH / float(font.height);
    float width = 0; unsigned n = 0;
    for (unsigned i = 0; i < length; ++i) if (const auto* g = glyph(font, text[i])) { width += float(g->advance) * scaleX; ++n; }
    if (n > 1) width += def.charSpace * float(n - 1);
    const float lineH = float(font.linefeed) * scaleY, cellH = float(font.cellH) * scaleY;
    const float left = -float(def.origin % 3) * pane.w * .5f, top = float(def.origin / 3) * pane.h * .5f;
    static constexpr float Align[3] = {0, .5f, 1};
    float x = left + (pane.w - width) * Align[def.textPosition % 3];
    const float y = top - (pane.h - lineH) * Align[std::min(def.textPosition / 3, 2)];
    const uint32_t topColor = withAlpha(pane.vertex[0], pane.alpha), bottomColor = withAlpha(pane.vertex[2], pane.alpha);
    for (unsigned i = 0; i < length; ++i) {
        const auto* g = glyph(font, text[i]); if (!g) continue;
        if (g->w > 0) {
            auto* q = batch.add(); if (!q) return;
            const float gx = x + float(g->left) * scaleX, gw = float(g->w) * scaleX;
            const float lx[4] = {gx, gx + gw, gx, gx + gw}, ly[4] = {y, y, y - cellH, y - cellH};
            const float u0 = float(g->x) / float(font.atlasW), u1 = float(g->x + g->w) / float(font.atlasW);
            const float v0 = float(g->y) / float(font.atlasH), v1 = float(g->y + font.cellH) / float(font.atlasH);
            const float us[4] = {u0, u1, u0, u1}, vs[4] = {v0, v0, v1, v1};
            for (unsigned k = 0; k < 4; ++k) { place(pane, at, lx[k], ly[k], q->x[k], q->y[k]); q->u[k] = us[k]; q->v[k] = vs[k]; q->color[k] = k < 2 ? topColor : bottomColor; }
        }
        x += float(g->advance) * scaleX + def.charSpace;
    }
}
// UTF-8 (the cooked texts) to UTF-16.
unsigned utf16(const char* text, char16_t* out, unsigned capacity) {
    unsigned n = 0;
    for (const auto* p = reinterpret_cast<const unsigned char*>(text); p && *p && n < capacity;) {
        uint32_t c = *p++;
        if (c >= 0xE0 && p[0] && p[1]) { c = (c & 0x0F) << 12 | (p[0] & 0x3F) << 6 | (p[1] & 0x3F); p += 2; }
        else if (c >= 0xC0 && p[0]) { c = (c & 0x1F) << 6 | (p[0] & 0x3F); p += 1; }
        out[n++] = char16_t(c);
    }
    return n;
}
Placement placement(const FlowControl& c) { return {c.x, c.y, c.sx, c.sy}; }
} // namespace

void AwardPageDrawer::flush(FlowSink sink, void* context, int texture, uint8_t wrapS, uint8_t wrapT, uint32_t c0, uint32_t c1) {
    if (!batch_.count) return;
    sink({texture, wrapS, wrapT, c0, c1, batch_.quads.data(), batch_.count}, context);
    batch_.count = 0;
}

void AwardPageDrawer::reset(const AwardPageView& view) {
    view_ = view; frame_ = 0; rows_ = {};
    const FlowLayoutDef& item = view.win ? AwardItemWinLayout : AwardItemLoseLayout;
    // fn_1_ABDFC: Loop 0, Player 1 (CPU), PlayerColorOn 1 (off), PlayerColorType 0, CPUColorType 0,
    // PlayerFlash 1 (CPU); then fn_1_AB81C per racer.
    static constexpr const char* Groups[6] = {"Loop", "Player", "PlayerColorOn", "PlayerColorType", "CPUColorType", "PlayerFlash"};
    static constexpr const char* Clips[6][6] = {{"Loop"}, {"Player", "CPU"}, {"PlayerColorOn", "PlayerColorOff"},
        {"PlayerColor1P", "PlayerColor2P", "PlayerColor3P", "PlayerColor4P", "PlayerColorRed", "PlayerColorBlue"},
        {"CPUColorOff", "CPUColorRed", "CPUColorBlue"}, {"FlashPlayer", "FlashCPU"}};
    for (const auto& r : view.rows) {
        if (!r.used || !r.standing || r.standing > 12) continue;
        auto& row = rows_[r.standing - 1];
        row.used = true;
        const unsigned player = r.human ? 0u : 1u;
        const unsigned choice[6] = {0, player, r.human ? 0u : 1u, r.human ? std::min<unsigned>(r.playerIndex, 3) : 0u, 0, player};
        for (unsigned g = 0; g < 6; ++g) row.plays[g] = flowPlay(item, Groups[g], Clips[g][choice[g]]);
    }
    const FlowLayoutDef& congrats = view.win ? AwardCongratulationsWinLayout : AwardCongratulationsLoseLayout;
    congrats_ = flowPlay(congrats, "Fade", "Hide");
}

void AwardPageDrawer::step() {
    const FlowLayoutDef& item = view_.win ? AwardItemWinLayout : AwardItemLoseLayout;
    for (auto& row : rows_) if (row.used) for (auto& p : row.plays) flowAdvance(item, p, 1);
    const FlowLayoutDef& congrats = view_.win ? AwardCongratulationsWinLayout : AwardCongratulationsLoseLayout;
    if (frame_ == CongratsStartFrame) congrats_ = flowPlay(congrats, "Fade", "Fadein");
    else flowAdvance(congrats, congrats_, 1);
    ++frame_;
}

bool AwardPageDrawer::stepFade(unsigned animation) {
    static constexpr const char* Names[4] = {"Running", "FadeEnter", "FadeExit", "Finish"};
    animation = std::min(animation, 3u);
    if (animation != fadeAnimation_ || fade_.clip < 0) { fadeAnimation_ = animation; fade_ = flowPlay(WinningRunFadeLayout, "Fade", Names[animation]); return false; }
    if (flowFinished(WinningRunFadeLayout, fade_)) return false;
    flowAdvance(WinningRunFadeLayout, fade_, 1);
    return flowFinished(WinningRunFadeLayout, fade_);
}

void AwardPageDrawer::drawFade(FlowSink sink, void* context) {
    pose_.evaluate(WinningRunFadeLayout, &fade_, 1);
    const Placement at = placement(WinningRunFadeControls[0]);
    for (unsigned i = 0; i < WinningRunFadeLayout.paneCount; ++i) {
        const auto& def = WinningRunFadeLayout.panes[i];
        const auto& pane = pose_.panes[i];
        if (def.kind != 1 || !pane.visible || pane.alpha <= 0 || def.material < 0) continue;
        const auto& m = pose_.materials[unsigned(def.material)];
        pictureQuad(batch_, def, pane, m, at);
        const auto& md = WinningRunFadeLayout.materials[unsigned(def.material)];
        flush(sink, context, md.texture, md.wrapS, md.wrapT, pack(m.c0), pack(m.c1));
    }
}

void AwardPageDrawer::draw(FlowSink sink, void* context) {
    char16_t text[64];
    // One control: every pane in draw order; text comes from `textOf`, pictures from their
    // material (cup and character icons from the atlases).
    auto control = [&](const FlowLayoutDef& def, const FlowControl& c, const FlowPlay* plays, unsigned playCount, auto&& textOf, int icon, int cup) {
        pose_.evaluate(def, plays, playCount);
        const Placement at = placement(c);
        for (unsigned i = 0; i < def.paneCount; ++i) {
            const auto& pd = def.panes[i];
            const auto& pane = pose_.panes[i];
            if (!pane.visible || pane.alpha <= 0) continue;
            if (pd.kind == 2) {
                const FlowFont* font = &AwardKanjiFont;
                const unsigned n = textOf(pd.name, text, 64u, font);
                if (n) { textQuads(batch_, pd, pane, at, text, n, *font); flush(sink, context, font->texture, 0, 0, 0x000000ffu, 0xffffffffu); }
            } else if (pd.kind == 1 && pd.material >= 0) {
                const auto& m = pose_.materials[unsigned(pd.material)];
                const auto& md = def.materials[unsigned(pd.material)];
                float rect[4];
                const float* uv = nullptr;
                if (md.texture == AwardIconTexture && icon >= -1) {
                    const auto& r = AwardIconRects[icon >= 0 && icon < 24 ? unsigned(icon) : 24u];
                    rect[0] = float(r[0]) / AwardIconAtlas[0]; rect[1] = float(r[1]) / AwardIconAtlas[1];
                    rect[2] = float(r[0] + r[2]) / AwardIconAtlas[0]; rect[3] = float(r[1] + r[3]) / AwardIconAtlas[1]; uv = rect;
                } else if (md.texture == AwardCupTexture && cup >= 0) {
                    const auto& r = AwardCupRects[unsigned(cup) % 8];
                    rect[0] = float(r[0]) / AwardCupAtlas[0]; rect[1] = float(r[1]) / AwardCupAtlas[1];
                    rect[2] = float(r[0] + r[2]) / AwardCupAtlas[0]; rect[3] = float(r[1] + r[3]) / AwardCupAtlas[1]; uv = rect;
                }
                pictureQuad(batch_, pd, pane, m, at, uv);
                flush(sink, context, md.texture, md.wrapS, md.wrapT, pack(m.c0), pack(m.c1));
            }
        }
    };
    auto none = [](const char*, char16_t*, unsigned, const FlowFont*&) { return 0u; };
    // AwardType (fn_1_AC628 0x805BC9DC): cup icon, "{cup} {class}" (0x58D).
    {
        auto typeText = [&](const char* pane, char16_t* out, unsigned cap, const FlowFont*&) -> unsigned {
            if (std::strcmp(pane, "cup_name") != 0) return 0;
            char line[96];
            const unsigned cls = view_.engineClass == 0 ? 0 : view_.engineClass == 1 ? 1 : view_.mirror ? 3 : 2;
            std::snprintf(line, sizeof line, "%s %s", AwardCupText[view_.cup % 8], AwardClassText[cls]);
            return utf16(line, out, cap);
        };
        control(view_.win ? AwardTypeWinLayout : AwardTypeLoseLayout, (view_.win ? AwardTypeWinControls : AwardTypeLoseControls)[0], nullptr, 0, typeText, -2, int(view_.cup % 8));
    }
    // AwardRank (fn_1_AC7EC): "RANK:" (control message 0x58F) and the grade glyph, GP only.
    if (view_.grandPrix) {
        auto rankText = [&](const char* pane, char16_t* out, unsigned cap, const FlowFont*& font) -> unsigned {
            if (std::strcmp(pane, "rank") == 0) return utf16(AwardRankText[0], out, cap);
            if (std::strcmp(pane, "star") == 0 && cap) { font = &AwardGradeFont; out[0] = AwardGradeGlyphs[std::min(view_.grade, 7u)].code; return 1; }
            return 0;
        };
        control(view_.win ? AwardRankWinLayout : AwardRankLoseLayout, (view_.win ? AwardRankWinControls : AwardRankLoseControls)[0], nullptr, 0, rankText, -2, -1);
    }
    // AwardCongratulations (fn_1_AC870): the trophy line, 0x4C3..0x4C6.
    {
        auto congratsText = [&](const char* pane, char16_t* out, unsigned cap, const FlowFont*&) -> unsigned {
            if (std::strcmp(pane, "text_00") != 0 && std::strcmp(pane, "text_light") != 0) return 0;
            return utf16(AwardTrophyText[gpTrophyMessage(view_.bestStanding) - 0x4C3], out, cap);
        };
        control(view_.win ? AwardCongratulationsWinLayout : AwardCongratulationsLoseLayout,
                (view_.win ? AwardCongratulationsWinControls : AwardCongratulationsLoseControls)[0], &congrats_, 1, congratsText, -2, -1);
    }
    // Twelve AwardItem rows, slot = standing - 1 (Pos0..Pos11).
    const FlowLayoutDef& item = view_.win ? AwardItemWinLayout : AwardItemLoseLayout;
    const FlowControl* slots = view_.win ? AwardItemWinControls : AwardItemLoseControls;
    for (const auto& r : view_.rows) {
        if (!r.used || !r.standing || r.standing > 12) continue;
        auto rowText = [&](const char* pane, char16_t* out, unsigned cap, const FlowFont*&) -> unsigned {
            if (std::strcmp(pane, "position") == 0) return utf16(AwardPositionText[r.standing - 1], out, cap);
            if (std::strcmp(pane, "mii_name") == 0) return utf16(r.name ? r.name : r.character < 24 ? AwardNameText[r.character] : "", out, cap);
            if (std::strcmp(pane, "point") == 0) { char n[8]; std::snprintf(n, sizeof n, "%u", unsigned(r.points)); return utf16(n, out, cap); }
            if (std::strcmp(pane, "pts") == 0) return utf16(AwardUnitText[r.points == 1 ? 0 : 1], out, cap);
            return 0;
        };
        const auto& row = rows_[r.standing - 1];
        control(item, slots[r.standing - 1], row.plays.data(), unsigned(row.plays.size()), rowText, r.character < 24 ? int(r.character) : -1, -1);
    }
    (void)none;
}

} // namespace mkw
