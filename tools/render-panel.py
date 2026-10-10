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

THE "SIGNAL FLOW" PANEL (branch UpdatedUI). The licence pill shows a
sample evaluation line; the real words come from LicenceState.

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

from PIL import Image, ImageDraw, ImageFont

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
GROUND = (20, 21, 24)
HEADER = (26, 28, 32)
BORDER = (42, 45, 51)
SECTION = (28, 30, 34)
RULE = (36, 38, 43)
TRACK = (46, 49, 55)
CAP = (42, 44, 49)
CAP_EDGE = (60, 64, 71)
TEXT = (236, 234, 228)
TITLE = (201, 204, 210)
MUTED = (156, 161, 170)
SUBTLE = (126, 131, 140)
DIM = (74, 78, 86)
SCREEN = (15, 16, 18)
SCREEN_EDGE = (38, 41, 46)
GRID = (26, 29, 33)
SEG_BACK = (18, 19, 22)
LAMP_ON = (255, 138, 61)
LAMP_OFF = (58, 42, 32)
LAMP_EDGE = (90, 58, 40)

ACCENT = {'osc': (242, 165, 65), 'filter': (86, 168, 245), 'amp': (95, 208, 160), 'mod': (176, 140, 255)}
INK = {'osc': (26, 18, 5), 'filter': (6, 20, 36), 'amp': (6, 30, 20), 'mod': (22, 15, 38)}


# ---------------------------------------------------------------------------
# Faces: the nearest this machine has to Helvetica Neue and Menlo
# ---------------------------------------------------------------------------
def face(candidates, size):
    for path in candidates:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, int(size * SS))
            except OSError:
                pass
    return ImageFont.load_default()

SANS = ['/System/Library/Fonts/HelveticaNeue.ttc', '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',
        '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf']
SANS_BOLD = ['/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf',
             '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf']
MONO = ['/System/Library/Fonts/Menlo.ttc', '/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf',
        '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf']
MONO_BOLD = ['/usr/share/fonts/truetype/liberation/LiberationMono-Bold.ttf',
             '/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf']
CONDENSED = ['/System/Library/Fonts/Avenir Next Condensed.ttc',
             '/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf']

F_WORDMARK = face(CONDENSED, 23)
F_TITLE = face(SANS_BOLD, 13)
F_SUBTITLE = face(SANS, 12)
F_LABEL = face(SANS_BOLD, 10)
F_VALUE = face(MONO, 10)
F_NOTE = face(SANS, 11)
F_BADGE = face(MONO_BOLD, 12)
F_SEGMENT = face(SANS_BOLD, 12)


# ---------------------------------------------------------------------------
# Drawing helpers, in panel pixels
# ---------------------------------------------------------------------------
def p(v):
    return v * SS


def box(r):
    return [p(r[0]), p(r[1]), p(r[2]), p(r[3])]


def text_in(d, r, msg, fill, fnt, align='left'):
    """VSTGUI's drawString in a rect: vertically centred."""
    y = p((r[1] + r[3]) / 2)
    if align == 'left':
        d.text((p(r[0]), y), msg, fill=fill, font=fnt, anchor='lm')
    elif align == 'right':
        d.text((p(r[2]), y), msg, fill=fill, font=fnt, anchor='rm')
    else:
        d.text((p((r[0] + r[2]) / 2), y), msg, fill=fill, font=fnt, anchor='mm')


def text_width(d, msg, fnt):
    return d.textlength(msg, font=fnt) / SS


def round_rect(d, r, radius, fill=None, outline=None):
    d.rounded_rectangle(box(r), radius=p(radius), fill=fill, outline=outline, width=SS if outline else 0)


def arc(d, cx, cy, radius, start, end, colour, width):
    if end - start < 0.5:
        return
    bb = [p(cx - radius - width / 2), p(cy - radius - width / 2), p(cx + radius + width / 2), p(cy + radius + width / 2)]
    d.arc(bb, start, end, fill=colour, width=p(width))
    import math
    for a in (start, end):       # round caps
        t = math.radians(a)
        x, y = cx + radius * math.cos(t), cy + radius * math.sin(t)
        d.ellipse([p(x - width / 2), p(y - width / 2), p(x + width / 2), p(y + width / 2)], fill=colour)


def disc(d, cx, cy, radius, fill=None, outline=None, width=1):
    d.ellipse([p(cx - radius), p(cy - radius), p(cx + radius), p(cy + radius)], fill=fill, outline=outline,
              width=p(width) if outline else 0)


# ---------------------------------------------------------------------------
# The panel
# ---------------------------------------------------------------------------
def backdrop(d, L):
    W, H, HH = L['width'], L['height'], L['headerHeight']
    d.rectangle([0, 0, p(W), p(H)], fill=GROUND)
    d.rectangle([0, 0, p(W), p(HH)], fill=HEADER)
    d.rectangle([0, p(HH - 1), p(W), p(HH)], fill=BORDER)

    wm = L['wordmark']
    text_in(d, wm, 'R2', TEXT, F_WORDMARK)
    w = text_width(d, 'R2', F_WORDMARK)
    text_in(d, [wm[0] + w + 1, wm[1], wm[2], wm[3]], 'SYNTH', ACCENT['osc'], F_WORDMARK)
    d.rectangle([p(wm[2] + 4), p(16), p(wm[2] + 5), p(40)], fill=BORDER)

    for s in L['sections']:
        r = s['rect']
        round_rect(d, r, L['sectionRadius'], fill=SECTION, outline=BORDER)
        top = r[1] + L['sectionPadTop']
        disc(d, r[0] + L['sectionPadX'] + 4, top + L['sectionTitleH'] / 2, 4, fill=ACCENT[s['accent']])
        tr = [r[0] + L['sectionPadX'] + 16, top, r[2] - L['sectionPadX'], top + L['sectionTitleH']]
        text_in(d, tr, s['title'], TITLE, F_TITLE)
        if s['subtitle']:
            w = text_width(d, s['title'], F_TITLE)
            text_in(d, [tr[0] + w + 10, tr[1], tr[2], tr[3]], s['subtitle'], SUBTLE, F_SUBTITLE)

    for x, y in L['chevrons']:
        d.line([p(x - 3), p(y - 8), p(x + 3), p(y), p(x - 3), p(y + 8)], fill=DIM, width=p(2), joint='curve')

    for r in L['rules']:
        d.rectangle([p(r[0]), p(r[1]), p(r[2]) - 1, p(r[3]) - 1], fill=RULE)

    for c in L['captions']:
        r, st = c['rect'], c['style']
        if st == 'block':
            text_in(d, r, c['text'].upper(), ACCENT[c['accent']], F_LABEL)
        elif st == 'field':
            text_in(d, r, c['text'].upper(), MUTED, F_LABEL)
        elif st == 'note':
            text_in(d, r, c['text'], SUBTLE, F_NOTE)
        else:
            disc(d, (r[0] + r[2]) / 2, (r[1] + r[3]) / 2, (r[3] - r[1]) / 2, fill=CAP)
            text_in(d, r, c['text'], ACCENT[c['accent']], F_BADGE, 'centre')


def knob(d, L, c):
    r, dia, acc = c['rect'], c['diameter'], ACCENT[c['accent']]
    stroke = 5 if dia >= 60 else 4
    cx, cy = (r[0] + r[2]) / 2, r[1] + dia / 2
    radius = dia / 2 - stroke / 2 - 1
    v = max(0.0, min(1.0, c['value']))
    a = 135 + 270 * v
    arc(d, cx, cy, radius, 135, 405, TRACK, stroke)
    if c['bipolar']:
        arc(d, cx, cy, radius, min(a, 270), max(a, 270), acc, stroke)
        if abs(a - 270) < 0.5:
            disc(d, cx, cy - radius, stroke / 2, fill=acc)
    else:
        arc(d, cx, cy, radius, 135, a, acc, stroke)
    cap = radius - stroke / 2 - 4
    disc(d, cx, cy, cap, fill=CAP, outline=CAP_EDGE)
    import math
    t = math.radians(a)
    d.line([p(cx + cap * 0.2 * math.cos(t)), p(cy + cap * 0.2 * math.sin(t)),
            p(cx + (cap - 2) * math.cos(t)), p(cy + (cap - 2) * math.sin(t))], fill=TEXT, width=p(2))
    top = r[1] + dia + L['knobGap']
    text_in(d, [r[0] - 4, top, r[2] + 4, top + L['knobValueH']], c['text'], TEXT, F_VALUE, 'centre')
    top += L['knobValueH']
    text_in(d, [r[0] - 4, top, r[2] + 4, top + L['knobLabelH']], c['label'].upper(), MUTED, F_LABEL, 'centre')


def switch(d, c):
    r, names = c['rect'], c['names']
    round_rect(d, r, 7, fill=SEG_BACK, outline=BORDER)
    n = len(names)
    cur = int(round(c['value'] * (n - 1))) if n > 1 else 0
    seg = (r[2] - r[0] - 4) / n
    for i, name in enumerate(names):
        sr = [r[0] + 2 + i * seg, r[1] + 2, r[0] + 2 + (i + 1) * seg, r[3] - 2]
        if i == cur:
            round_rect(d, sr, 5, fill=ACCENT[c['accent']])
            text_in(d, sr, name, INK[c['accent']], F_SEGMENT, 'centre')
        else:
            text_in(d, sr, name, MUTED, F_SEGMENT, 'centre')


def screen(img, d, s):
    r = s['rect']
    acc = ACCENT[s['accent']]
    round_rect(d, r, 8, fill=SCREEN)
    for x in range(r[0] + 24, r[2] - 1, 24):
        d.rectangle([p(x), p(r[1] + 1), p(x + 1) - 1, p(r[3] - 1)], fill=GRID)
    for y in range(r[1] + 24, r[3] - 1, 24):
        d.rectangle([p(r[0] + 1), p(y), p(r[2] - 1), p(y + 1) - 1], fill=GRID)
    inset, th = 6, 13
    tr = [r[0] + inset + 2, r[1] + 3, r[2] - inset - 2, r[1] + 3 + th]
    if s['caption']:
        text_in(d, tr, s['caption'], SUBTLE, F_VALUE)
    if s['annotation']:
        text_in(d, tr, s['annotation'], SUBTLE, F_VALUE, 'right')
    fr = [r[0] + inset + 2, r[3] - 3 - th, r[2] - inset - 2, r[3] - 3]
    if s['legend'][0]:
        text_in(d, fr, s['legend'][0], acc, F_VALUE)
    if s['legend'][1]:
        text_in(d, fr, s['legend'][1], DIM if s['accent'] == 'mod' else MUTED, F_VALUE, 'right')

    pl, pt, pr, pb = r[0] + inset, r[1] + inset, r[2] - inset, r[3] - inset
    if s['caption'] or s['annotation']:
        pt += th
    if s['legend'][0] or s['legend'][1]:
        pb -= th

    def pts(data):
        n = len(data)
        return [(p(pl + (pr - pl) * i / (n - 1)), p(pb - max(0.0, min(1.0, v)) * (pb - pt))) for i, v in enumerate(data)]

    if 0 <= s['gate'] <= 1:
        x = pl + s['gate'] * (pr - pl)
        y = pt
        while y < pb:
            d.line([p(x), p(y), p(x), p(min(y + 3, pb))], fill=DIM, width=SS)
            y += 6

    if s['companion']:
        d.line(pts(s['companion']), fill=DIM if s['accent'] == 'mod' else MUTED, width=int(p(1.5)), joint='curve')
    main = pts(s['main'])
    if s['fill']:
        overlay = Image.new('RGBA', img.size, (0, 0, 0, 0))
        od = ImageDraw.Draw(overlay)
        od.polygon([(p(pl), p(pb))] + main + [(p(pr), p(pb))], fill=acc + (40,))
        img.alpha_composite(overlay)
    d.line(main, fill=acc, width=p(2), joint='curve')
    if s['marker'][0] >= 0:
        mx = pl + s['marker'][0] * (pr - pl)
        my = pb - max(0.0, min(1.0, s['marker'][1])) * (pb - pt)
        disc(d, mx, my, 4.5, fill=SCREEN, outline=acc, width=2)
    round_rect(d, r, 8, outline=SCREEN_EDGE)


def lamp(d, L):
    r, on = L['lamp']['rect'], L['lamp']['on']
    w = text_width(d, 'SELF-OSC', F_LABEL)
    left = r[2] - w
    cy = (r[1] + r[3]) / 2
    if on:
        disc(d, left - 10, cy, 4, fill=LAMP_ON)
    else:
        disc(d, left - 10, cy, 4, fill=LAMP_OFF, outline=LAMP_EDGE)
    text_in(d, [left, r[1], r[2], r[3]], 'SELF-OSC', LAMP_ON if on else SUBTLE, F_LABEL)


def pill(d, L):
    r = L['badge']
    msg = 'Evaluation - 7 days left - activate...'
    w = min(r[2] - r[0], text_width(d, msg, F_TITLE) + 40)
    pr = [r[2] - w, r[1], r[2], r[3]]
    round_rect(d, pr, (r[3] - r[1]) / 2, fill=(42, 36, 24), outline=(74, 59, 30))
    disc(d, pr[0] + 16, (r[1] + r[3]) / 2, 4, fill=ACCENT['osc'])
    text_in(d, [pr[0] + 28, pr[1], pr[2], pr[3]], msg, (242, 194, 122), F_TITLE)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'docs', 'panel.png')
    L = dump_layout()
    img = Image.new('RGBA', (p(L['width']), p(L['height'])), GROUND + (255,))
    d = ImageDraw.Draw(img)
    backdrop(d, L)
    for s in L['screens']:
        screen(img, d, s)
        d = ImageDraw.Draw(img)
    lamp(d, L)
    text_in(d, L['engineLine']['rect'], L['engineLine']['text'], SUBTLE, F_SUBTITLE)
    text_in(d, L['info']['rect'], L['info']['text'], SUBTLE, F_VALUE)
    for c in L['controls']:
        if c['kind'] == 'knob':
            knob(d, L, c)
        else:
            switch(d, c)
    pill(d, L)
    img = img.convert('RGB').resize((L['width'] * SCALE, L['height'] * SCALE), Image.LANCZOS)
    img.save(out)
    print('wrote %s (%dx%d)' % (out, img.width, img.height))


if __name__ == '__main__':
    main()
