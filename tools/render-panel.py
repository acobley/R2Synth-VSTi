#!/usr/bin/env python3
"""Draw the plug-in's panel into docs/panel.png.

WHY IT DOES NOT PARSE THE SOURCE. This script asks the plug-in:
tools/dump-layout.cpp is compiled against source/R2SynthLayout.h,
R2SynthReadouts.cpp and R2SynthDsp.cpp and prints every position,
every piece of text and all four screens' curves as JSON - from the SAME
code the editor runs. This script only draws. A layout change that breaks
the tool fails to compile rather than drawing last week's panel.

What it cannot show is VSTGUI's own rendering: this is a reconstruction
from the same numbers in the panel's palette (R2SynthPanelViews.h),
not a screenshot - and the faces are whatever this machine has nearest to
Helvetica Neue and Menlo. It exists so that a control in the wrong place
HERE is a control in the wrong place in the plug-in - the only way to
judge a layout without a host.

THE "SIGNAL FLOW" PANEL (branch UpdatedUI), dressed as THE BLACK PANEL
(branch PolivoksUI, 22 Oct 2026): white printing on black, knurled knobs,
chicken-head selectors, small colour OLED screens, and every letter in the
70s sci-fi lettering - stroked from the glyphs dump-layout prints, parsed
by SciFiLettering.h, so this script keeps no copy of the face. Faces are
no longer this machine's business: the panel draws no font text. The
licence pill shows a sample evaluation line; the real words come from
LicenceState.

    python3 tools/render-panel.py [out.png]

Needs a C++ compiler, Pillow, and the VST3 SDK for one header-only file:
external/vst3sdk (the in-tree clone), or VST3_SDK_ROOT in the environment.
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile

import math

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))
SCALE = 2          # the PNG is drawn at 2x so the text is legible
SS = 4             # ...from a 4x drawing, for smooth curves


# ---------------------------------------------------------------------------
# Ask the plug-in
# ---------------------------------------------------------------------------
def dump_layout():
    sdk = os.environ.get('VST3_SDK_ROOT') or os.path.join(ROOT, 'external', 'vst3sdk')
    if not os.path.exists(os.path.join(sdk, 'pluginterfaces', 'vst', 'vsttypes.h')):
        raise SystemExit('render-panel.py: no VST3 SDK at %s - run ./setup-xcode.sh once, '
                         'or set VST3_SDK_ROOT' % sdk)
    cxx = shutil.which('c++') or shutil.which('g++') or shutil.which('clang++')
    if not cxx:
        raise SystemExit('render-panel.py: no C++ compiler. Refusing to draw the panel '
                         'from a second copy of its numbers.')
    src = os.path.join(ROOT, 'source')
    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, 'dump-layout')
        subprocess.run([cxx, '-std=c++17', '-O1', '-I' + sdk, '-I' + src, '-o', exe,
                        os.path.join(ROOT, 'tools', 'dump-layout.cpp'),
                        os.path.join(src, 'R2SynthReadouts.cpp'),
                        os.path.join(src, 'R2SynthParams.cpp'),
                        os.path.join(src, 'R2SynthDsp.cpp')], check=True)
        out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout
    return json.loads(out)


# ---------------------------------------------------------------------------
# The palette - R2SynthPanelViews.h
# ---------------------------------------------------------------------------
GROUND = (18, 18, 18)
HEADER = (12, 12, 12)
HEADER_RULE = (233, 230, 220)
SECTION = (21, 21, 21)
SECTION_EDGE = (217, 214, 204)
INK = (244, 241, 232)
INK_SOFT = (185, 182, 174)
INK_DIM = (143, 140, 133)
DIVIDER = (90, 88, 82)
RULE = (58, 58, 58)
TICK = (233, 230, 220)
RED = (226, 52, 43)
SCREW = (58, 58, 58)
KNOB_BODY = (10, 10, 10)
KNOB_RIM = (47, 47, 47)
BEZEL = (5, 5, 5)
BEZEL_EDGE = (60, 60, 60)
GRID_DOT = (42, 45, 51)
SCREEN_TEXT = (141, 150, 163)
LAMP_ON = (255, 59, 47)
LAMP_OFF = (58, 21, 18)

TRACE = {'osc': INK, 'filter': (47, 184, 255), 'amp': (56, 240, 160), 'mod': (194, 123, 255)}


# ---------------------------------------------------------------------------
# Drawing helpers, in panel pixels
# ---------------------------------------------------------------------------
def p(v):
    return v * SS


def pt(x, y):
    return (p(x), p(y))


def from_top(cx, cy, r, deg):
    """The layout's angles: degrees from twelve o'clock, clockwise."""
    t = math.radians(deg)
    return cx + r * math.sin(t), cy - r * math.cos(t)


def vstgui_angle(cx, cy, r, deg):
    """VSTGUI's angles: 0 at three o'clock, clockwise (y down)."""
    t = math.radians(deg)
    return cx + r * math.cos(t), cy + r * math.sin(t)


def stroke(d, pts, colour, width):
    """A polyline with round caps and joins."""
    if len(pts) == 1:
        pts = pts * 2
    d.line([pt(x, y) for x, y in pts], fill=colour, width=max(1, round(p(width))))
    rr = width / 2
    for x, y in pts:
        d.ellipse([p(x - rr), p(y - rr), p(x + rr), p(y + rr)], fill=colour)


def disc(d, cx, cy, radius, fill=None, outline=None, width=1.0):
    d.ellipse([p(cx - radius), p(cy - radius), p(cx + radius), p(cy + radius)], fill=fill, outline=outline,
              width=max(1, round(p(width))) if outline else 0)


def round_rect(d, r, radius, fill=None, outline=None, width=1.0):
    d.rounded_rectangle([p(r[0]), p(r[1]), p(r[2]), p(r[3])], radius=p(radius), fill=fill, outline=outline,
                        width=max(1, round(p(width))) if outline else 0)


def quad(p0, q, p2, steps=8):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * q[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * q[1] + t * t * p2[1])
            for t in (i / steps for i in range(1, steps + 1))]


def blend(c1, c2, t):
    return tuple(int(round(a + (b - a) * t)) for a, b in zip(c1, c2))


# ---------------------------------------------------------------------------
# The lettering - the glyphs as dump-layout printed them
# ---------------------------------------------------------------------------
L = None


def code_points(text):
    out = []
    for ch in text:
        c = ord(ch)
        if ord('a') <= c <= ord('z'):
            c -= 32
        elif 0xC0 <= c <= 0xFF and c not in (0xD7, 0xF7):
            c = ord('Y') if c == 0xFF else ord('AAAAAAACEEEEIIIIDNOOOOO*OUUUUYTS'[(c & ~0x20) - 0xC0])
        out.append(c)
    return out


def glyph(c):
    return L['glyphs'].get(str(c)) or L['glyphs']['missing']


def placed(text, style):
    x, out = 0.0, []
    for c in code_points(text):
        if c == 32:
            x += L['space']
            continue
        out.append((c, x))
        x += glyph(c)[0] + style['weight'] + style['track']
    return out, x


def text_width(text, style):
    _, end = placed(text, style)
    return (end - style['track']) * style['cap'] / 6 if end > 0 else 0.0


def lettering(d, text, style, colour, x, cy, align='left', stroke_units=None):
    s = style['cap'] / 6
    w = text_width(text, style)
    left = x - w if align == 'right' else (x - w / 2 if align == 'centre' else x)
    top = cy - (style['cap'] + style['weight'] * s) / 2
    half = style['weight'] / 2
    lw = (stroke_units or style['weight']) * s
    glyphs, _ = placed(text, style)
    for c, gx in glyphs:
        ox, oy = left + (gx + half) * s, top + half * s
        at = lambda u, v: (ox + u * s, oy + v * s)
        line, start = [], None
        for op, x1, y1, ex, ey in glyph(c)[1]:
            if op == 'M':
                if len(line) > 0:
                    stroke(d, line, colour, lw)
                line = [at(ex, ey)]
                start = line[0]
            elif op == 'L':
                line.append(at(ex, ey))
            elif op == 'Q':
                line += quad(line[-1], at(x1, y1), at(ex, ey))
            elif op == 'Z':
                line.append(start)
        if line:
            stroke(d, line, colour, lw)


# ---------------------------------------------------------------------------
# The panel
# ---------------------------------------------------------------------------
def backdrop(d):
    W, H, HH = L['width'], L['height'], L['headerHeight']
    S = L['styles']
    d.rectangle([0, 0, p(W), p(H)], fill=GROUND)
    d.rectangle([0, 0, p(W), p(HH)], fill=HEADER)
    d.rectangle([0, p(HH - 2), p(W), p(HH)], fill=HEADER_RULE)

    for x, y in L['screws']:
        disc(d, x, y, 3.2, fill=SCREW, outline=(0, 0, 0), width=0.6)
        stroke(d, [(x - 2.2, y - 2.2), (x + 2.2, y + 2.2)], (34, 34, 34), 0.8)

    wm = L['wordmark']
    cy = HH / 2 + 1
    second = wm[0] + text_width('R2', S['wordmark']) + 12
    for ink, units in ((INK, None), (HEADER, 0.32)):
        lettering(d, 'R2', S['wordmark'], ink, wm[0], cy, stroke_units=units)
        lettering(d, 'SYNTH', S['wordmark'], ink, second, cy, stroke_units=units)
    dv = L['headerDivider']
    d.rectangle([p(dv), p(16), p(dv + 1), p(40)], fill=DIVIDER)

    for sec in L['sections']:
        r = sec['rect']
        round_rect(d, [r[0] + 0.5, r[1] + 0.5, r[2] - 0.5, r[3] - 0.5], L['sectionRadius'], fill=SECTION,
                   outline=SECTION_EDGE, width=1.2)
        b = sec['titleBox']
        round_rect(d, b, 2, fill=SECTION, outline=SECTION_EDGE, width=1.2)
        c = (b[1] + b[3]) / 2
        lettering(d, sec['title'], S['title'], INK, b[0] + 8, c)
        if sec['subtitle']:
            lettering(d, sec['subtitle'], S['subtitle'], INK_SOFT, sec['subtitleLeft'], c)

    for x, y in L['chevrons']:
        stroke(d, [(x - 5, y - 10), (x + 4, y), (x - 5, y + 10)], SECTION_EDGE, 2.2)

    for r in L['rules']:
        d.rectangle([p(r[0]), p(r[1]), p(r[2]) - 1, p(r[3]) - 1], fill=RULE)

    bz = L['screenBezel']
    for s in L['screens']:
        r = s['rect']
        round_rect(d, [r[0] - bz + 0.5, r[1] - bz + 0.5, r[2] + bz - 0.5, r[3] + bz - 0.5], 4, fill=BEZEL,
                   outline=BEZEL_EDGE, width=1)

    for c in L['captions']:
        r, st = c['rect'], c['style']
        cy = (r[1] + r[3]) / 2
        if st == 'block':
            lettering(d, c['text'], S['block'], INK, r[0], cy)
        elif st == 'field':
            lettering(d, c['text'], S['label'], INK, r[0], cy)
        elif st == 'note':
            lettering(d, c['text'], S['note'], INK_SOFT, r[0], cy)
        else:
            bs = L['badgeSize']
            x, y = (r[0] + r[2]) / 2, r[1] + bs / 2
            disc(d, x, y, bs / 2 - 1, outline=SECTION_EDGE, width=1.4)
            lettering(d, c['text'], S['badge'], INK, x, y, 'centre')
            lettering(d, 'VCO', S['badgeSub'], INK, x, y + 20, 'centre')


def cap(d, cx, cy, radius):
    """A black cap lit from the top left: concentric discs, the light
    drifting towards the highlight as they shrink."""
    steps = 24
    for i in range(steps):
        t = i / (steps - 1)
        rr = radius * (1 - t * 0.85)
        ox, oy = -radius * 0.25 * t, -radius * 0.35 * t
        col = blend((5, 5, 5), (22, 22, 22), min(1, t / 0.4)) if t < 0.4 else blend((22, 22, 22), (58, 58, 58), (t - 0.4) / 0.6)
        disc(d, cx + ox, cy + oy, rr, fill=col)


def knob(d, c):
    S = L['styles']
    r, dia = c['rect'], c['diameter']
    cx, cy = (r[0] + r[2]) / 2, r[1] + dia / 2
    outer = dia / 2
    body = outer - (6 if dia >= 44 else 5)
    a = 135 + 270 * max(0.0, min(1.0, c['value']))

    for i in range(11):
        t = 135 + 27 * i
        long_ = i in (0, 5, 10)
        stroke(d, [vstgui_angle(cx, cy, body + 1.5, t), vstgui_angle(cx, cy, outer - (0 if long_ else 1.8), t)],
               TICK, 1.3 if long_ else 0.9)
    if dia >= 52:
        lo = from_top(cx, cy, outer + 2, -138)
        hi = from_top(cx, cy, outer + 2, 138)
        lettering(d, '-' if c['bipolar'] else '0', S['tick'], TICK, lo[0] - 2, lo[1] + 4, 'right')
        lettering(d, '+' if c['bipolar'] else '10', S['tick'], TICK, hi[0] + 2, hi[1] + 4)

    teeth = [vstgui_angle(cx, cy, body if i % 2 == 0 else body - 1.4, 360 * i / 56) for i in range(56)]
    d.polygon([pt(x, y) for x, y in teeth], fill=KNOB_BODY, outline=KNOB_RIM)
    cap(d, cx, cy, body * 0.78)
    disc(d, cx, cy, body * 0.78, outline=(0, 0, 0), width=0.8)
    stroke(d, [vstgui_angle(cx, cy, body * 0.15, a), vstgui_angle(cx, cy, body - 0.5, a)], INK,
           2.2 if dia >= 44 else 1.8)

    top = r[1] + dia + L['knobGap']
    lettering(d, c['text'], S['value'], INK, cx, top + L['knobValueH'] / 2, 'centre')
    lettering(d, c['label'], S['label'], INK, cx, top + L['knobValueH'] + L['knobLabelH'] / 2, 'centre')


def selector(d, c):
    S = L['styles']
    g = c['selector']
    cx, cy, rad = g['cx'], g['cy'], g['radius']
    names = c['names']
    cur = int(round(c['value'] * (len(names) - 1)))
    for i, name in enumerate(names):
        t = g['angles'][i]
        on = i == cur
        stroke(d, [from_top(cx, cy, rad + 2.5, t), from_top(cx, cy, rad + 8, t)], RED if on else TICK, 2 if on else 1.6)
        b = g['labelBoxes'][i]
        lettering(d, name, S['label'], INK if on else INK_DIM, b[0], (b[1] + b[3]) / 2)

    a = g['angles'][cur]
    disc(d, cx, cy, rad * 0.8, fill=(11, 11, 11), outline=(68, 68, 68), width=0.8)
    tip = from_top(cx, cy, rad + 3, a)
    left = from_top(cx, cy, 8, a - 90)
    right = from_top(cx, cy, 8, a + 90)
    tail = from_top(cx, cy, rad, a + 180)
    beak = [tip, right] + quad(right, tail, left)
    d.polygon([pt(x, y) for x, y in beak], fill=(26, 26, 26))
    stroke(d, beak + [tip], (0, 0, 0), 0.8)
    stroke(d, [from_top(cx, cy, 2, a), tip], INK, 1.8)
    lettering(d, c['label'], S['label'], INK, cx, g['nameY'], 'centre')


def screen(img, s):
    S = L['styles']
    r = s['rect']
    trace = TRACE[s['accent']]
    d = ImageDraw.Draw(img)
    round_rect(d, r, 2, fill=(0, 0, 0))
    for x in range(r[0] + 24, r[2] - 1, 24):
        for y in range(r[1] + 12, r[3] - 1, 12):
            disc(d, x, y, 0.6, fill=GRID_DOT)
    for y in range(r[1] + 24, r[3] - 1, 24):
        for x in range(r[0] + 12, r[2] - 1, 12):
            disc(d, x, y, 0.6, fill=GRID_DOT)

    if s['caption']:
        lettering(d, s['caption'], S['screen'], SCREEN_TEXT, r[0] + 8, r[1] + 10)
    if s['annotation']:
        lettering(d, s['annotation'], S['screen'], trace, r[2] - 8, r[1] + 10, 'right')
    if s['legend'][0]:
        lettering(d, s['legend'][0], S['screen'], trace, r[0] + 8, r[3] - 9)
    if s['legend'][1]:
        lettering(d, s['legend'][1], S['screen'], SCREEN_TEXT, r[2] - 8, r[3] - 9, 'right')

    inset, th = 6, 13
    pl, ptop, pr, pb = r[0] + inset, r[1] + inset, r[2] - inset, r[3] - inset
    if s['caption'] or s['annotation']:
        ptop += th
    if s['legend'][0] or s['legend'][1]:
        pb -= th

    def pts(data):
        n = len(data)
        return [(pl + (pr - pl) * i / (n - 1), pb - max(0.0, min(1.0, v)) * (pb - ptop)) for i, v in enumerate(data)]

    if 0 <= s['gate'] <= 1:
        x = pl + s['gate'] * (pr - pl)
        y = ptop
        while y < pb:
            stroke(d, [(x, y), (x, min(y + 3, pb))], SCREEN_TEXT, 1)
            y += 6

    if s['companion']:
        stroke(d, pts(s['companion']), blend((0, 0, 0), SCREEN_TEXT, 0.6), 1.4)

    main = pts(s['main'])
    overlay = Image.new('RGBA', img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    if s['fill']:
        # The fill fades from the line's colour at the top of the plot to
        # black at the floor: a vertical gradient through a polygon mask.
        mask = Image.new('L', img.size, 0)
        ImageDraw.Draw(mask).polygon([pt(pl, pb)] + [pt(x, y) for x, y in main] + [pt(pr, pb)], fill=255)
        grad = Image.new('RGBA', img.size, (0, 0, 0, 0))
        gd = ImageDraw.Draw(grad)
        for yy in range(int(p(ptop)), int(p(pb)) + 1):
            t = (yy - p(ptop)) / max(1, p(pb) - p(ptop))
            gd.line([(p(pl), yy), (p(pr), yy)], fill=trace + (int(97 + (5 - 97) * t),))
        overlay.paste(grad, (0, 0), mask)
    stroke(od, main, trace + (60,), 4.1)       # the bloom
    img.alpha_composite(overlay)
    d = ImageDraw.Draw(img)
    stroke(d, main, trace, 1.6)
    if s['marker'][0] >= 0:
        mx = pl + s['marker'][0] * (pr - pl)
        my = pb - max(0.0, min(1.0, s['marker'][1])) * (pb - ptop)
        disc(d, mx, my, 4, fill=(0, 0, 0), outline=INK, width=1.6)
    # the glass: a hairline highlight along the top
    sheen = Image.new('RGBA', img.size, (0, 0, 0, 0))
    stroke(ImageDraw.Draw(sheen), [(r[0] + 3, r[1] + 0.8), (r[2] - 3, r[1] + 0.8)], (255, 255, 255, 30), 0.6)
    img.alpha_composite(sheen)


def lamp(d):
    S = L['styles']
    r, on = L['lamp']['rect'], L['lamp']['on']
    cy = (r[1] + r[3]) / 2
    left = r[2] - text_width('SELF-OSC', S['label'])
    x = left - 12
    disc(d, x, cy, 5.5, fill=(0, 0, 0), outline=SECTION_EDGE, width=1)
    disc(d, x, cy, 4, fill=LAMP_ON if on else LAMP_OFF)
    lettering(d, 'SELF-OSC', S['label'], INK, r[2], cy, 'right')


def pill(d):
    S = L['styles']
    r = L['badge']
    msg = L['pillText']
    w = min(r[2] - r[0], text_width(msg, S['pill']) + 46)
    pr = [r[2] - w, r[1], r[2], r[3]]
    round_rect(d, [pr[0] + 0.6, pr[1] + 0.6, pr[2] - 0.6, pr[3] - 0.6], 3, fill=(12, 12, 12), outline=RED, width=1.2)
    cy = (r[1] + r[3]) / 2
    disc(d, pr[0] + 16, cy, 4.5, fill=RED)
    lettering(d, msg, S['pill'], INK, pr[0] + 30, cy)


def main():
    global L
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'docs', 'panel.png')
    L = dump_layout()
    img = Image.new('RGBA', (p(L['width']), p(L['height'])), GROUND + (255,))
    d = ImageDraw.Draw(img)
    backdrop(d)
    for s in L['screens']:
        screen(img, s)
    d = ImageDraw.Draw(img)
    lamp(d)
    S = L['styles']
    er = L['engineLine']['rect']
    lettering(d, L['engineLine']['text'], S['engine'], INK, er[0], (er[1] + er[3]) / 2)
    ir = L['info']['rect']
    lettering(d, L['info']['text'], S['info'], INK, ir[0], (ir[1] + ir[3]) / 2)
    for c in L['controls']:
        if c['kind'] == 'knob':
            knob(d, c)
        else:
            selector(d, c)
    pill(d)
    img = img.convert('RGB').resize((L['width'] * SCALE, L['height'] * SCALE), Image.LANCZOS)
    img.save(out)
    print('wrote %s (%dx%d)' % (out, img.width, img.height))


if __name__ == '__main__':
    main()
