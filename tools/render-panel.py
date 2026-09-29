#!/usr/bin/env python3
"""Draw the plug-in's panel into docs/panel.png.

WHY IT DOES NOT PARSE THE SOURCE. FilterDrum's render-panel.py read the
editor's constants with regular expressions. This one asks the plug-in:
tools/dump-layout.cpp is compiled against source/SimpleSynthLayout.h,
SimpleSynthReadouts.cpp and SimpleSynthDsp.cpp and prints every position,
every piece of text and both displays' curves as JSON - from the SAME code
the editor runs. This script only draws. A layout change that breaks the
tool fails to compile rather than drawing last week's panel.

What it cannot show is VSTGUI's own rendering: this is a reconstruction
from the same numbers in FilterDrum's palette, not a screenshot. It exists
so that a control in the wrong place HERE is a control in the wrong place
in the plug-in - the only way to judge a layout without a host.

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
SCALE = 2          # drawn at 2x so the text is legible


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
                        os.path.join(src, 'SimpleSynthReadouts.cpp'),
                        os.path.join(src, 'SimpleSynthParams.cpp'),
                        os.path.join(src, 'SimpleSynthDsp.cpp')], check=True)
        out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout
    return json.loads(out)


P = dump_layout()

# ---------------------------------------------------------------------------
# The palette - SimpleSynthControls.h (FilterDrum's) and the editor's ground
# ---------------------------------------------------------------------------
BACK    = (44, 48, 44)
LABEL   = (50, 255, 50)
VALUE   = (232, 232, 232)
TRACE   = (127, 200, 255)
BAR_HI  = (255, 255, 255)
BAR_LO  = (200, 200, 200)
BAR_FL  = (100, 100, 100)
LAMP    = (255, 0, 0)
OUTER   = (100, 100, 100)
G_FRAME = (120, 128, 120)
T_VCF   = (60, 255, 90)
T_VCA   = (255, 70, 70)
PLATE   = (12, 13, 12)
COLOURS = {'trace': TRACE, 'value': VALUE, 'label': LABEL}


def font(size):
    for path in ('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
                 '/System/Library/Fonts/Supplemental/Arial.ttf',
                 '/Library/Fonts/Arial.ttf'):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


F_MAIN = font(8 * SCALE)
F_SMALL = font(7 * SCALE)
F_TINY = font(6 * SCALE)

img = Image.new('RGB', (P['width'] * SCALE, P['height'] * SCALE), BACK)
d = ImageDraw.Draw(img)


def s(v):
    return int(round(v * SCALE))


def text(x, y, msg, fill=VALUE, fnt=F_MAIN, anchor='la'):
    d.text((s(x), s(y)), msg, fill=fill, font=fnt, anchor=anchor)


def fitted(cx, y, width, msg, fill):
    """drawFitted: the main face, dropping a size rather than overflowing."""
    for fnt in (F_MAIN, F_SMALL, F_TINY):
        if d.textlength(msg, font=fnt) <= s(width):
            break
    text(cx, y, msg, fill=fill, fnt=fnt, anchor='ma')


def bevel(x0, y0, x1, y1, light, dark):
    """draw3dRect: light top-left, dark bottom-right."""
    d.line([(s(x0), s(y0)), (s(x1), s(y0))], fill=light, width=SCALE)
    d.line([(s(x0), s(y0)), (s(x0), s(y1))], fill=light, width=SCALE)
    d.line([(s(x0), s(y1)), (s(x1), s(y1))], fill=dark, width=SCALE)
    d.line([(s(x1), s(y0)), (s(x1), s(y1))], fill=dark, width=SCALE)


def group_box(rect, title):
    """SpyGroupBox: a rectangle with its name breaking the top edge.

    The rect is the VIEW; the frame's top edge is titleBand down it
    (setTitleBand), and the title is centred on that edge. Everything is
    drawn inside the view - VSTGUI clips a view to its rectangle, and this
    script used not to, which is how a title cut in half in the plug-in
    looked whole in the picture."""
    x0, y0, x1, y1 = rect
    clip_top = y0
    y0 = y0 + P['titleBand']
    tw = d.textlength(title, font=F_SMALL) / SCALE
    gap0, gap1 = x0 + 4, x0 + 4 + tw + 8
    d.line([s(x0), s(y0), s(gap0), s(y0)], fill=G_FRAME, width=1)
    d.line([s(gap1), s(y0), s(x1) - 1, s(y0)], fill=G_FRAME, width=1)
    d.line([s(x0), s(y1) - 1, s(x1) - 1, s(y1) - 1], fill=G_FRAME, width=1)
    d.line([s(x0), s(y0), s(x0), s(y1) - 1], fill=G_FRAME, width=1)
    d.line([s(x1) - 1, s(y0), s(x1) - 1, s(y1) - 1], fill=G_FRAME, width=1)
    # Centred on the edge, and - like VSTGUI - never above the view.
    text(x0 + 8, max(y0 - 5.5, clip_top), title, fill=G_FRAME, fnt=F_SMALL)


def slider(c):
    """SpySlider: reading at the top, bar at bottom-15..bottom-3, label in
    the band from bottom-17 - over the bar, as the DXi drew it."""
    x0, y0, x1, y1 = c['rect']
    w = x1 - x0
    fill_w = w * c['value']
    if fill_w >= 2:
        bevel(x0, y1 - 15, x0 + fill_w, y1 - 3, BAR_LO, BAR_HI)
        d.rectangle([s(x0) + SCALE, s(y1 - 15) + SCALE, s(x0 + fill_w) - SCALE, s(y1 - 3) - SCALE],
                    fill=BAR_FL)
    fitted(x0 + w / 2, y1 - 17, w, c['label'], LABEL)
    fitted(x0 + w / 2, y0 + 1, w, c['text'], VALUE)
    if c['lamp']:
        d.rectangle([s(x0), s(y0), s(x0 + 10), s(y0 + 10)],
                    fill=LAMP if c['lit'] else (0, 0, 0), outline=BAR_FL)


def selector(c):
    """SpySelector, compact (setCompact): an outlined box exactly where a
    slider's bar is, the position's name where a slider's reading is, and
    the label under it where a slider's label is."""
    x0, y0, x1, y1 = c['rect']
    w = x1 - x0
    bevel(x0, y1 - 15, x1, y1 - 3, BAR_LO, BAR_HI)
    fitted(x0 + w / 2, y1 - 17, w, c['label'], LABEL)
    fitted(x0 + w / 2, y0 + 1, w, c['text'], VALUE)


def display(v):
    """SpyEnvelopeView: plate, caption and annotation on top, legend in the
    trace colours at the bottom, floor line, gate dots, then the curves."""
    x0, y0, x1, y1 = v['rect']
    d.rectangle([s(x0), s(y0), s(x1) - 1, s(y1) - 1], fill=PLATE, outline=OUTER)
    text(x0 + 4, y0 + 3, v['caption'], fill=VALUE, fnt=F_TINY)
    text(x1 - 4, y0 + 3, v['annotation'], fill=BAR_LO, fnt=F_TINY, anchor='ra')
    text(x0 + 4, y1 - 14, v['legend'][0], fill=T_VCF, fnt=F_TINY)
    text(x1 - 4, y1 - 14, v['legend'][1], fill=T_VCA, fnt=F_TINY, anchor='ra')

    left, width = x0 + 2, (x1 - x0) - 4
    top, height = y0 + 2 + 12, (y1 - y0) - 4 - 24
    d.line([s(left), s(top + height), s(left + width), s(top + height)], fill=OUTER, width=1)
    if 0 <= v['gate'] <= 1:
        gx = left + width * v['gate']
        y = top
        while y < top + height:
            d.line([s(gx), s(y), s(gx), s(min(y + 2, top + height))], fill=BAR_LO, width=1)
            y += 4

    for key, colour in (('a', T_VCF), ('b', T_VCA)):
        pts = v[key]
        n = len(pts)
        xy = [(s(left + width * i / (n - 1)), s(top + height - max(0.0, min(1.0, p)) * height))
              for i, p in enumerate(pts)]
        d.line(xy, fill=colour, width=SCALE)


# ---------------------------------------------------------------------------
# The two fixed lines come from SimpleSynthReadouts.cpp via the dump, like
# everything else, so the picture says what the editor says.
text(P['contentX'], P['titleY'], P['title'], fill=VALUE)
text(P['contentX'], P['flowY'], P['flow'], fill=LABEL)

for g in P['groups']:
    group_box(g['rect'], g['title'])
for c in P['controls']:
    (selector if c['kind'] == 'selector' else slider)(c)
display(P['lfoView'])
display(P['envView'])
for r in P['readouts']:
    text(r['x'], r['y'] + 2, r['text'], fill=COLOURS[r['colour']], fnt=F_SMALL)

out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'docs', 'panel.png')
os.makedirs(os.path.dirname(out), exist_ok=True)
img.save(out)
print('render-panel.py: wrote %s (%d x %d at %dx)' % (out, P['width'], P['height'], SCALE))
