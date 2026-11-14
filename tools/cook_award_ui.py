#!/usr/bin/env python3
"""Cook the retail Grand Prix award screens from the locally extracted Award UI archive.

Scene/UI/Award.szs: award/ (page 0x3C: AwardType, AwardRank, AwardCongratulations and twelve
AwardItem controls, Win and Lose variants; PAL onInit fn_1_ABDFC 0x805BC1B0) and winning_run/
(page 0x3B: WinningRunFade, fn_1_AB008 0x805BB3BC). Messages come from Race_U/Common.bmg, the
fonts from Font.szs (assets/race-ui/Font).

Outputs: the layout geometry, animation keys, glyph tables and texts as
include/mkw/award_layout.hpp (or --header), and textures under <output>/textures (an overlay
that tools/package_sd.py adds, like build/race-flow). The shared sd-card is never written.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT / 'tools'), str(ROOT / 'vendor/gclib')]
from cook_race_flow import (ICONS, RACE_CHARACTERS, controller_clips, controller_variants, digest, emit_layout, f,
                            font_glyphs, pack_atlas, parse_layout, texture_file)
from cook_race_items import decode_tpl
from menu_layout import controller_animations, messages

AWARD = ROOT / 'assets/menu-ui/Award'
RACE_UI = ROOT / 'assets/race-ui'
# Cup icons by cup id (fn_1_3507A0): the Special Cup's icon is the crown (oukan).
CUPS = ['kinoko', 'flower', 'star', 'oukan', 'koura', 'banana', 'konoha', 'thunder']
# (directory, control, C++ prefix). Win and Lose controls of one layout share its tables.
CONTROLS = [('award', 'AwardTypeWin'), ('award', 'AwardTypeLose'), ('award', 'AwardRankWin'), ('award', 'AwardRankLose'),
            ('award', 'AwardCongratulationsWin'), ('award', 'AwardCongratulationsLose'),
            ('award', 'AwardItemWin'), ('award', 'AwardItemLose'), ('winning_run', 'WinningRunFade')]
# Race.bmg 0x590..0x597 draw these picture-font glyphs (escape group 1, tag 0): stars, then A-E.
GRADE_GLYPHS = [0xF063, 0xF062, 0xF061, 0xF078, 0xF079, 0xF07A, 0xF07B, 0xF07C]


def cpp_string(text):
    return '"' + ''.join(c if c not in '"\\' else '\\' + c for c in text) + '"'


def cook(out, header_path):
    (out / 'textures').mkdir(parents=True, exist_ok=True)
    sources = []
    textures = []

    def add(name, image, fmt):
        textures.append(texture_file(image, name, out, fmt))
        return len(textures) - 1

    race = messages(RACE_UI / 'Race_U/message/Race.bmg')
    common = messages(RACE_UI / 'Race_U/message/Common.bmg')
    sources += [RACE_UI / 'Race_U/message/Race.bmg', RACE_UI / 'Race_U/message/Common.bmg']

    # Layouts, clips and control variants.
    layouts, controls = {}, []
    for directory, control in CONTROLS:
        base = AWARD / directory
        ctrl = base / 'ctrl' / (control + '.brctr')
        name = controller_animations(ctrl)['layout']
        if name not in layouts:
            layout = parse_layout(base / 'blyt' / (name + '.brlyt'))
            layouts[name] = {'layout': layout, 'clips': controller_clips(ctrl, layout, base / 'anim'), 'dir': base}
            sources.append(base / 'blyt' / (name + '.brlyt'))
            sources += sorted({c['source'] for c in layouts[name]['clips']})
        elif [(c['group'], c['name']) for c in controller_clips(ctrl, layouts[name]['layout'], base / 'anim')] != \
                [(c['group'], c['name']) for c in layouts[name]['clips']]:
            raise ValueError(f'{control} binds other clips than its layout twin')
        sources.append(ctrl)
        controls.append((control, name, controller_variants(ctrl)))
    for name, entry in layouts.items():
        if len(entry['layout']['panes']) > 24 or len(entry['layout']['materials']) > 16:
            raise ValueError(f'{name} exceeds FlowPose (24 panes, 16 materials)')

    # Textures: the layouts' own images, the character icon atlas (AwardItem chara_icon is
    # replaced per row, getCharacterPane) and the cup icon atlas (AwardType cup_icon).
    slots = {}
    for name, entry in layouts.items():
        for m in entry['layout']['materials']:
            tex = m['texture']
            if not tex or tex in slots or tex in ('tt_mario_64x64.tpl', 'tt_cup_icon_kinoko_00.tpl'):
                continue
            path = entry['dir'] / 'timg' / tex
            sources.append(path)
            slots[tex] = add('award_' + Path(tex).stem, decode_tpl(path), 'rgba8')
    timg = AWARD / 'award/timg'
    icons = {i: decode_tpl(timg / f'tt_{ICONS[i]}_64x64.tpl') for i in RACE_CHARACTERS}
    icons[-1] = decode_tpl(timg / 'tt_hatena_64x64.tpl')
    sources += [timg / f'tt_{ICONS[i]}_64x64.tpl' for i in RACE_CHARACTERS] + [timg / 'tt_hatena_64x64.tpl']
    icon_atlas, icon_rects = pack_atlas(icons, padding=0)
    slots['tt_mario_64x64.tpl'] = add('award_icons', icon_atlas, 'rgba8')
    cups = {i: decode_tpl(timg / f'tt_cup_icon_{c}_00.tpl') for i, c in enumerate(CUPS)}
    sources += [timg / f'tt_cup_icon_{c}_00.tpl' for c in CUPS]
    cup_atlas, cup_rects = pack_atlas(cups, padding=0)
    slots['tt_cup_icon_kinoko_00.tpl'] = add('award_cups', cup_atlas, 'rgba8')

    # Texts (USA English): cup names Common.bmg 0x23F0+, class 0x589-0x58C, trophy lines
    # 0x4C3-0x4C6, "RANK:" 0x58F, positions 0x515+, names 0x2328+, pt/pts 0x524/0x525.
    texts = {
        'cup': [common[0x23F0 + i] for i in range(8)],
        'class': [race[0x589 + i] for i in range(4)],
        'trophy': [race[0x4C3 + i] for i in range(4)],
        'rank': [race[0x58F]],
        'position': [race[0x515 + i] for i in range(12)],
        'name': [common[0x2328 + i] for i in range(24)],
        'unit': [race[0x524], race[0x525]],
    }
    # Every one of these messages carries BMG font attribute 1 (BMGHolder::GetFontIndex): the
    # kart_kanji_font message font, whatever font the layout's text pane names.
    kanji_text = ''.join(sum(texts.values(), [])) + '0123456789'
    fonts = []
    for key, font_name, text in (('Kanji', 'kart_kanji_font', kanji_text),):
        path = RACE_UI / f'Font/{font_name}.brfnt'
        sources.append(path)
        info, glyphs = font_glyphs(path, text.replace(' ', '') + ' ')
        atlas, rects = pack_atlas({c: g['image'] for c, g in glyphs.items()})
        fonts.append((key, info, glyphs, rects, atlas.size, add(f'award_font_{key.lower()}', atlas, 'la4')))
    # The grade glyphs (picture font): taken as images, drawn as quads.
    extension = RACE_UI / 'Font/tt_kart_extension_font.brfnt'
    sources.append(extension)
    info, grade_glyphs = font_glyphs(extension, ''.join(chr(c) for c in GRADE_GLYPHS))
    grade_atlas, grade_rects = pack_atlas({c: g['image'] for c, g in grade_glyphs.items()})
    grade_slot = add('award_grades', grade_atlas, 'rgba8')

    def texture_slot(name):
        return slots[name]

    header = ['// Generated by tools/cook_award_ui.py from the retail Award UI archive (Scene/UI/Award.szs).',
              '#pragma once', '#include <array>', '#include <cstdint>', '#include <mkw/race_flow_layout.hpp>',
              'namespace mkw {',
              f'inline constexpr std::array<const char*,{len(textures)}> AwardTextures{{{{{",".join(cpp_string(t) for t in textures)}}}}};']
    for name, entry in layouts.items():
        prefix = 'Award' + ''.join(part.capitalize() for part in name.split('_'))
        entry['prefix'] = prefix
        emit_layout(prefix, entry['layout'], entry['clips'], texture_slot, header)
    header.append('// Control variants (16:9 transform x, y, sx, sy and page-slide delay).')
    for control, name, variants in controls:
        header.append(f'inline constexpr FlowControl {control}Controls[]={{')
        for vname, v in variants.items():
            header.append(f'    {{{f(v["x"])},{f(v["y"])},{f(v["sx"])},{f(v["sy"])},{f(v["delay"])}}}, // {vname}')
        header.append('};')
        header.append(f'inline constexpr const FlowLayoutDef& {control}Layout={layouts[name]["prefix"]}Layout;')
    for key, info, glyphs, rects, size, slot in fonts:
        header.append(f'inline constexpr FlowGlyph Award{key}Glyphs[]={{')
        for c in sorted(glyphs):
            x, y, w, _ = rects[c]
            g = glyphs[c]
            header.append(f'    {{u\'{c if c not in chr(39) + chr(92) else chr(92) + c}\',{x},{y},{g["width"]},{g["left"]},{g["advance"]}}},')
        header.append('};')
        header.append(f'inline constexpr FlowFont Award{key}Font{{{slot},{size[0]},{size[1]},{info["cellH"]},{info["width"]},'
                      f'{info["height"]},{info["linefeed"]},Award{key}Glyphs,{len(glyphs)}}};')
    header.append(f'inline constexpr int8_t AwardIconTexture={slots["tt_mario_64x64.tpl"]},AwardCupTexture={slots["tt_cup_icon_kinoko_00.tpl"]};')
    header.append('inline constexpr std::array<std::array<int16_t,4>,25> AwardIconRects{{')
    for i in list(range(24)) + [-1]:
        r = icon_rects[i]
        header.append(f'    {{{{{r[0]},{r[1]},{r[2]},{r[3]}}}}},')
    header.append('}};')
    header.append(f'inline constexpr uint16_t AwardIconAtlas[2]={{{icon_atlas.size[0]},{icon_atlas.size[1]}}};')
    header.append('inline constexpr std::array<std::array<int16_t,4>,8> AwardCupRects{{' +
                  ','.join(f'{{{{{r[0]},{r[1]},{r[2]},{r[3]}}}}}' for r in (cup_rects[i] for i in range(8))) + '}};')
    header.append(f'inline constexpr uint16_t AwardCupAtlas[2]={{{cup_atlas.size[0]},{cup_atlas.size[1]}}};')
    header.append('// Grade glyphs of tt_kart_extension_font (Race.bmg 0x590 + grade): 0 three stars .. 7 E.')
    header.append('inline constexpr FlowGlyph AwardGradeGlyphs[]={')
    for c in GRADE_GLYPHS:
        x, y, _, _ = grade_rects[chr(c)]
        g = grade_glyphs[chr(c)]
        header.append(f'    {{char16_t(0x{c:04X}),{x},{y},{g["width"]},{g["left"]},{g["advance"]}}},')
    header.append('};')
    header.append(f'inline constexpr FlowFont AwardGradeFont{{{grade_slot},{grade_atlas.size[0]},{grade_atlas.size[1]},{info["cellH"]},'
                  f'{info["width"]},{info["height"]},{info["linefeed"]},AwardGradeGlyphs,{len(GRADE_GLYPHS)}}};')
    for key, values in texts.items():
        header.append(f'inline constexpr std::array<const char*,{len(values)}> Award{key.capitalize()}Text{{{{{",".join(cpp_string(v) for v in values)}}}}};')
    header.append('}')
    header_path.write_text('\n'.join(header) + '\n')
    manifest = {'version': 1, 'textures': textures, 'sources': {str(p.relative_to(ROOT)).replace('\\', '/'): digest(p) for p in sources}}
    (out / 'award-ui.json').write_text(json.dumps(manifest, indent=1) + '\n')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/award-ui')
    parser.add_argument('--header', type=Path, default=ROOT / 'include/mkw/award_layout.hpp')
    args = parser.parse_args()
    manifest = cook(args.output, args.header)
    print(json.dumps({'textures': len(manifest['textures']), 'header': str(args.header)}))


if __name__ == '__main__':
    main()
