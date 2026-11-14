#!/usr/bin/env python3
"""Generate the waifu sprite set (7 chars x 7 emotions, 96x128 RGB565A8 with a
transparent background) plus the main-screen scene backdrop (320x140 RGB565).

Style: detailed semi-realistic anime - taller, near-real proportions, layered
hair with strand lines and shine bands, three-tone eyes with multiple
highlights, soft alpha shading on skin and clothes, and per-character outfits
with seams, folds and accessories. PNGs in assets/ are PREVIEWS ONLY; the
firmware uses the C arrays emitted into components/ui/src/sprites.c.

Run from the project root:  python3 tools/gen_sprites.py
"""
import os
from PIL import Image, ImageDraw

W, H = 96, 128
S = 4
BG = (24, 18, 43)  # must match kBgColor in VaiwuUi.cpp

SKIN = (247, 220, 194)
SKIN_SHADE = (233, 198, 168)
SKIN_SICK = (223, 226, 200)
OUTLINE = (92, 66, 74)
WHITE = (250, 246, 240)
BLUSH = (245, 175, 168)
MOUTH = (176, 88, 92)
LASH = (86, 60, 70)

EMOTIONS = ["idle", "happy", "sad", "angry", "sleepy", "sick", "dead"]
CHARS = ["aoi", "hina", "rin", "yuki", "akane", "unicorn", "kolobok"]

STYLE = {
    "aoi":  dict(hair=(150, 186, 224), hair_dk=(120, 158, 200), iris=(96, 148, 208),
                 top=(196, 216, 236), bottom=(170, 196, 224), accent=(224, 236, 248),
                 hairstyle="long", outfit="sundress"),
    "hina": dict(hair=(240, 190, 150), hair_dk=(216, 160, 116), iris=(196, 138, 76),
                 top=(246, 214, 186), bottom=(238, 196, 160), accent=(252, 232, 210),
                 hairstyle="twintails", outfit="hoodie"),
    "rin":  dict(hair=(232, 168, 176), hair_dk=(206, 138, 150), iris=(196, 106, 124),
                 top=(244, 210, 216), bottom=(150, 120, 150), accent=(250, 230, 234),
                 hairstyle="bob", outfit="sailor"),
    "yuki": dict(hair=(206, 194, 232), hair_dk=(178, 164, 212), iris=(148, 122, 210),
                 top=(224, 214, 240), bottom=(206, 194, 230), accent=(238, 230, 248),
                 hairstyle="hime", outfit="kimono"),
    "akane": dict(hair=(238, 186, 150), hair_dk=(212, 154, 110), iris=(204, 142, 72),
                  top=(240, 206, 176), bottom=(214, 176, 140), accent=(250, 234, 214),
                  hairstyle="fox", outfit="foxhoodie"),
    # Unicorn girl: pastel lilac mane + golden horn (see draw_front_hair).
    "unicorn": dict(hair=(224, 186, 242), hair_dk=(198, 156, 226), iris=(150, 112, 226),
                    top=(236, 222, 248), bottom=(210, 190, 238), accent=(248, 226, 170),
                    hairstyle="unicorn", outfit="sundress"),
    # Kolobok: not humanoid - a round golden bread bun with a simple face
    # (special-cased in render()). hair=bun crust, hair_dk=crust shading.
    "kolobok": dict(hair=(242, 206, 130), hair_dk=(212, 168, 88), iris=(150, 110, 60),
                    top=(242, 206, 130), bottom=(212, 168, 88), accent=(252, 238, 200),
                    hairstyle="kolobok", outfit="none"),
}


def shade(c, f):
    return tuple(max(0, int(v * f)) for v in c[:3])


def tint(c, f):
    return tuple(min(255, int(v + (255 - v) * f)) for v in c[:3])


def fade(c, a):
    return c[:3] + (a,)


def gray(c):
    l = int(0.30 * c[0] + 0.59 * c[1] + 0.11 * c[2])
    return (l, l, l) + tuple(c[3:])


def rgba(c):
    # Sprites are drawn on a transparent RGBA canvas; make every color
    # explicit-alpha so PIL never guesses.
    return c if len(c) == 4 else c + (255,)


class Painter:
    # dy shifts every y coordinate: used to recenter the shared face code on
    # non-humanoid bodies (kolobok's face sits lower than the girls').
    # The ImageDraw is expected to be created with mode "RGBA" so 4-tuple
    # colors alpha-blend onto the canvas (soft shadows, gloss, blush).
    def __init__(self, draw, dead, dy=0):
        self.d = draw
        self.dead = dead
        self.dy = dy

    def c(self, color):
        return rgba(gray(color) if self.dead else color)

    def xy(self, x, y):
        return (x * S, (y + self.dy) * S)

    def box(self, x0, y0, x1, y1):
        return [x0 * S, (y0 + self.dy) * S, x1 * S, (y1 + self.dy) * S]

    def ell(self, x0, y0, x1, y1, fill, outline=None, ow=0):
        self.d.ellipse(self.box(x0, y0, x1, y1), fill=self.c(fill),
                       outline=self.c(outline) if outline else None,
                       width=int(ow * S) if outline else 0)

    def poly(self, pts, fill, outline=None, ow=0):
        self.d.polygon([self.xy(x, y) for x, y in pts], fill=self.c(fill),
                       outline=self.c(outline) if outline else None,
                       width=int(ow * S) if outline else 0)

    def rrect(self, x0, y0, x1, y1, r, fill, outline=None, ow=0):
        self.d.rounded_rectangle(self.box(x0, y0, x1, y1), radius=r * S,
                                 fill=self.c(fill),
                                 outline=self.c(outline) if outline else None,
                                 width=int(ow * S) if outline else 0)

    def arc(self, x0, y0, x1, y1, a0, a1, color, w=1.0):
        self.d.arc(self.box(x0, y0, x1, y1), a0, a1, fill=self.c(color),
                   width=max(1, int(w * S)))

    def line(self, x0, y0, x1, y1, color, w=1.0):
        self.d.line([self.xy(x0, y0), self.xy(x1, y1)], fill=self.c(color),
                    width=max(1, int(w * S)))

    def chord(self, x0, y0, x1, y1, a0, a1, fill):
        self.d.chord(self.box(x0, y0, x1, y1), a0, a1, fill=self.c(fill))


HEAD = dict(x0=33, y0=7, x1=63, y1=45)


def draw_back_hair(p, st):
    hair, hair_dk, style = st["hair"], st["hair_dk"], st["hairstyle"]

    def strands(xs, y0, y1, color=None, w=0.5):
        for sx in xs:
            p.line(sx, y0, sx - 1, y1, color or hair_dk, w)

    if style == "long":
        p.ell(27, 6, 69, 50, hair_dk)
        p.rrect(28, 34, 68, 96, 9, hair_dk)
        p.ell(27, 84, 45, 102, hair_dk)
        p.ell(51, 84, 69, 102, hair_dk)
        p.rrect(30, 36, 66, 92, 8, hair)
        strands((31.5, 35, 61, 64.5), 44, 92)
        p.arc(29, 38, 41, 96, 60, 160, tint(hair, 0.25), 0.8)
        p.arc(55, 38, 67, 96, 20, 120, tint(hair, 0.25), 0.8)
    elif style == "twintails":
        p.ell(28, 6, 68, 48, hair_dk)
        p.ell(19, 12, 33, 28, hair_dk)                 # left puff (high)
        p.ell(63, 12, 77, 28, hair_dk)                 # right puff (high)
        p.rrect(18, 24, 28, 88, 5, hair_dk)            # left tail
        p.rrect(68, 24, 78, 88, 5, hair_dk)            # right tail
        p.ell(17, 82, 29, 96, hair_dk)                 # tail tips
        p.ell(67, 82, 79, 96, hair_dk)
        p.rrect(20, 26, 26, 84, 4, hair)               # inner volume
        p.rrect(70, 26, 76, 84, 4, hair)
        strands((21.5, 24, 72, 74.5), 30, 84)
        p.line(21, 30, 20, 80, tint(hair, 0.3), 0.7)   # edge shine
        p.line(71, 30, 70, 80, tint(hair, 0.3), 0.7)
        # little bows where the ties sit
        p.poly([(19, 22), (23, 18), (25, 23)], st["accent"], OUTLINE, 0.3)
        p.poly([(27, 22), (23, 18), (21, 23)], st["accent"], OUTLINE, 0.3)
        p.ell(21, 21, 25, 25, shade(st["accent"], 0.85))
        p.poly([(69, 22), (73, 18), (75, 23)], st["accent"], OUTLINE, 0.3)
        p.poly([(77, 22), (73, 18), (71, 23)], st["accent"], OUTLINE, 0.3)
        p.ell(71, 21, 75, 25, shade(st["accent"], 0.85))
    elif style == "bob":
        p.ell(27, 6, 69, 52, hair_dk)
        p.rrect(25, 30, 71, 58, 10, hair_dk)
        p.ell(24, 46, 40, 64, hair_dk)                 # curled-in tips
        p.ell(56, 46, 72, 64, hair_dk)
        p.rrect(27, 32, 69, 55, 9, hair)
        p.arc(28, 30, 44, 62, 90, 200, tint(hair, 0.25), 0.8)
        p.arc(52, 30, 68, 62, 340, 90, tint(hair, 0.25), 0.8)
    elif style == "hime":
        p.ell(27, 5, 69, 48, hair_dk)
        p.rrect(27, 32, 69, 106, 3, hair_dk)           # straight fall, blunt hem
        p.rrect(25, 30, 34, 78, 2, hair_dk)            # blunt side locks
        p.rrect(62, 30, 71, 78, 2, hair_dk)
        p.rrect(29, 34, 67, 102, 3, hair)
        p.rrect(26.5, 32, 32.5, 76, 1.5, hair)
        p.rrect(63.5, 32, 69.5, 76, 1.5, hair)
        p.line(27, 105, 69, 105, hair_dk, 0.7)         # blunt cut line
        strands((31, 34.5, 61.5, 65), 40, 102)
        p.line(30, 38, 29.5, 100, tint(hair, 0.3), 0.7)
        p.line(66, 38, 65.5, 100, tint(hair, 0.3), 0.7)
    elif style == "fox":
        p.ell(27, 6, 69, 50, hair_dk)
        p.rrect(27, 34, 67, 94, 9, hair_dk)
        p.rrect(29, 36, 65, 90, 8, hair)
        strands((30.5, 33.5, 62.5, 65.5), 44, 90)
        # fox ears with dark tips and cream fluff inside
        p.poly([(25, 20), (32, -4), (43, 24)], hair, OUTLINE, 0.4)
        p.poly([(53, 24), (64, -4), (71, 20)], hair, OUTLINE, 0.4)
        p.poly([(28, 8), (32, -4), (36, 10)], hair_dk)
        p.poly([(60, 10), (64, -4), (68, 8)], hair_dk)
        p.poly([(30, 18), (34, 4), (39, 21)], st["accent"])
        p.poly([(57, 21), (62, 4), (66, 18)], st["accent"])
        # big tail on the right with a cream tip and shading
        p.ell(60, 76, 94, 116, hair_dk, OUTLINE, 0.5)
        p.ell(64, 80, 90, 112, hair)
        p.arc(66, 82, 88, 110, 90, 220, hair_dk, 0.8)
        p.ell(76, 94, 92, 114, WHITE, OUTLINE, 0.3)
    elif style == "unicorn":
        # long flowing mane with a wavy hem, sweeping curl and pastel streaks
        p.ell(27, 6, 69, 50, hair_dk)
        p.rrect(28, 34, 68, 96, 9, hair_dk)
        for i, wx in enumerate(range(27, 63, 9)):
            p.ell(wx, 90 + (i % 2) * 4, wx + 11, 102 + (i % 2) * 4, hair_dk)
        p.ell(58, 82, 86, 108, hair_dk)                # mane tip curling out
        p.rrect(30, 36, 66, 92, 8, hair)
        streak = tint(hair_dk, 0.3)
        p.rrect(33, 42, 37, 90, 2.5, streak)           # pastel streaks
        p.rrect(59, 44, 63, 94, 2.5, streak)
        p.rrect(66, 58, 70, 92, 2, st["top"])
        p.arc(61, 84, 84, 106, 180, 330, tint(hair, 0.25), 0.9)


def draw_legs(p, st, skin):
    skin_sh = shade(skin, 0.9)
    shoe = shade(st["bottom"], 0.62)
    shoe_dk = shade(st["bottom"], 0.45)
    p.rrect(40, 96, 46, 118, 2.5, skin, OUTLINE, 0.4)
    p.rrect(50, 96, 56, 118, 2.5, skin, OUTLINE, 0.4)
    p.line(45, 100, 45, 112, skin_sh, 0.6)             # inner-leg shading
    p.line(51, 100, 51, 112, skin_sh, 0.6)
    # socks with a cuff stripe
    p.rrect(40, 104, 46, 118, 2, st["accent"])
    p.rrect(50, 104, 56, 118, 2, st["accent"])
    p.line(40.5, 106.5, 45.5, 106.5, shade(st["accent"], 0.8), 0.6)
    p.line(50.5, 106.5, 55.5, 106.5, shade(st["accent"], 0.8), 0.6)
    # mary-jane shoes: strap, sole and a gloss dot
    p.rrect(37, 115, 47, 124, 3, shoe, OUTLINE, 0.4)
    p.rrect(49, 115, 59, 124, 3, shoe, OUTLINE, 0.4)
    p.rrect(37, 121, 47, 124, 1.5, shoe_dk)
    p.rrect(49, 121, 59, 124, 1.5, shoe_dk)
    p.line(39, 117.5, 45, 117.5, shoe_dk, 0.6)
    p.line(51, 117.5, 57, 117.5, shoe_dk, 0.6)
    p.ell(39, 116.2, 41, 118.2, fade(tint(shoe, 0.5), 140))
    p.ell(51, 116.2, 53, 118.2, fade(tint(shoe, 0.5), 140))


def draw_arms(p, st, skin, sleeve):
    # sleeve: "puff", "short", "long" or "wide" (kimono handled separately)
    top, bottom = st["top"], st["bottom"]
    if sleeve == "puff":
        for sx in (28, 60):
            p.ell(sx, 50, sx + 9.5, 61, top, OUTLINE, 0.4)
            p.chord(sx, 56, sx + 9.5, 62, 0, 180, shade(top, 0.85))
        p.rrect(30, 60, 36, 90, 2.5, skin, OUTLINE, 0.4)
        p.rrect(60, 60, 66, 90, 2.5, skin, OUTLINE, 0.4)
        p.ell(29.5, 88, 36.5, 96, skin, OUTLINE, 0.4)
        p.ell(59.5, 88, 66.5, 96, skin, OUTLINE, 0.4)
    elif sleeve == "short":
        p.rrect(29, 52, 38, 66, 3, top, OUTLINE, 0.4)
        p.rrect(58, 52, 67, 66, 3, top, OUTLINE, 0.4)
        p.line(29.5, 63.5, 37.5, 63.5, bottom, 0.8)    # sleeve stripes
        p.line(58.5, 63.5, 66.5, 63.5, bottom, 0.8)
        p.rrect(30.5, 66, 36, 90, 2.5, skin, OUTLINE, 0.4)
        p.rrect(60, 66, 65.5, 90, 2.5, skin, OUTLINE, 0.4)
        p.ell(29.5, 88, 36.5, 96, skin, OUTLINE, 0.4)
        p.ell(59.5, 88, 66.5, 96, skin, OUTLINE, 0.4)
    elif sleeve == "long":
        p.rrect(28.5, 52, 37, 88, 4, top, OUTLINE, 0.4)
        p.rrect(59, 52, 67.5, 88, 4, top, OUTLINE, 0.4)
        p.line(32, 56, 31.5, 82, shade(top, 0.88), 0.5)  # sleeve crease
        p.line(64, 56, 64.5, 82, shade(top, 0.88), 0.5)
        p.rrect(28.5, 84, 37, 92, 2.5, bottom, OUTLINE, 0.3)  # ribbed cuffs
        p.rrect(59, 84, 67.5, 92, 2.5, bottom, OUTLINE, 0.3)
        p.ell(30, 90, 37, 97, skin, OUTLINE, 0.4)
        p.ell(59, 90, 66, 97, skin, OUTLINE, 0.4)


def draw_body(p, st, emo):
    skin = SKIN_SICK if emo == "sick" else SKIN
    top, bottom, accent, outfit = st["top"], st["bottom"], st["accent"], st["outfit"]
    top_sh, top_hi = shade(top, 0.85), tint(top, 0.25)
    bot_sh = shade(bottom, 0.82)

    # neck + soft shadow cast by the chin
    p.rrect(44, 42, 52, 54, 3, skin)
    p.chord(43, 41, 53, 50, 0, 180, fade(shade(SKIN_SHADE, 0.85), 90))

    draw_legs(p, st, skin)

    if outfit == "sundress":
        p.rrect(39, 47, 42, 54, 1, top, OUTLINE, 0.3)   # shoulder straps
        p.rrect(54, 47, 57, 54, 1, top, OUTLINE, 0.3)
        p.poly([(36, 52), (60, 52), (57, 76), (39, 76)], top, OUTLINE, 0.5)
        p.arc(38, 62, 58, 76, 20, 160, top_sh, 0.7)     # under-bust seam
        p.line(41, 54, 44, 72, top_hi, 0.6)             # bodice sheen
        p.rrect(38, 74, 58, 79, 2, top_sh)              # waistband
        p.poly([(38, 79), (58, 79), (67, 103), (29, 103)], top, OUTLINE, 0.5)
        for fx0, fx1 in ((37, 33), (43, 41), (53, 55), (59, 63)):  # radiating folds
            p.line(fx0, 80, fx1, 101, top_sh, 0.6)
        p.rrect(29, 99, 67, 105, 3, bottom)             # hem band
        p.line(31, 101.5, 65, 101.5, top_hi, 0.5)       # hem stitching
        draw_arms(p, st, skin, "puff")
    elif outfit in ("hoodie", "foxhoodie"):
        p.ell(35, 44, 61, 60, top_sh)                   # hood resting behind neck
        p.arc(39, 46, 57, 62, 20, 160, shade(top, 0.75), 1.0)
        p.rrect(33, 50, 63, 92, 7, top, OUTLINE, 0.5)
        p.line(48, 58, 48, 90, top_sh, 0.6)             # center seam
        p.line(44.5, 56, 43.5, 67, accent, 0.8)         # drawstrings
        p.line(51.5, 56, 52.5, 67, accent, 0.8)
        p.ell(42.5, 66, 45.5, 69, accent)
        p.ell(50.5, 66, 53.5, 69, accent)
        p.poly([(38, 74), (58, 74), (56, 88), (40, 88)], shade(top, 0.93), OUTLINE, 0.3)
        p.line(38, 74, 42, 80, OUTLINE, 0.3)            # pocket slants
        p.line(58, 74, 54, 80, OUTLINE, 0.3)
        if outfit == "foxhoodie":                       # paw print on the pocket
            p.ell(46, 80, 50, 84, accent)
            p.ell(43.5, 77.5, 45.5, 79.8, accent)
            p.ell(47, 76.8, 49, 79.2, accent)
            p.ell(50.5, 77.5, 52.5, 79.8, accent)
        p.rrect(33, 88, 63, 96, 4, bottom, OUTLINE, 0.4)  # ribbed hem
        for hx in range(36, 63, 4):
            p.line(hx, 89, hx, 95, bot_sh, 0.5)
        draw_arms(p, st, skin, "long")
    elif outfit == "sailor":
        p.rrect(35, 50, 61, 82, 5, top, OUTLINE, 0.5)
        p.poly([(34, 50), (62, 50), (57, 68), (39, 68)], bottom, OUTLINE, 0.4)
        p.line(37.5, 53, 41, 65.5, accent, 0.7)         # collar stripes
        p.line(58.5, 53, 55, 65.5, accent, 0.7)
        p.line(36, 51, 60, 51, accent, 0.7)
        p.poly([(42, 50), (54, 50), (48, 64)], top)     # chest V under the collar
        p.ell(44.5, 54, 51.5, 59, shade(accent, 0.85), OUTLINE, 0.3)  # scarf knot
        p.poly([(43, 58), (53, 58), (48, 73)], accent, OUTLINE, 0.3)
        p.line(48, 60, 48, 71, shade(accent, 0.8), 0.5)
        p.line(37, 74, 59, 74, top_sh, 0.5)             # blouse hem seam
        p.poly([(35, 82), (61, 82), (67, 102), (29, 102)], bottom, OUTLINE, 0.5)
        for fx in range(33, 66, 6):                     # pleats
            p.line(fx, 84, fx - 2, 100, bot_sh, 0.7)
        for fx in range(39, 66, 12):
            p.line(fx, 84, fx - 1.5, 100, tint(bottom, 0.18), 0.5)
        draw_arms(p, st, skin, "short")
    elif outfit == "kimono":
        p.poly([(35, 50), (61, 50), (67, 112), (29, 112)], top, OUTLINE, 0.5)
        p.poly([(44, 50), (52, 50), (48, 58)], skin)    # collar opening
        p.poly([(48, 50), (60, 50), (49, 80)], accent, OUTLINE, 0.3)
        p.poly([(48, 50), (36, 50), (47, 80)], shade(accent, 0.85), OUTLINE, 0.3)
        p.rrect(33, 78, 63, 90, 2, bottom, OUTLINE, 0.4)  # obi
        p.line(33, 84, 63, 84, accent, 0.7)             # obijime cord
        p.ell(44, 80, 52, 88, accent, OUTLINE, 0.3)     # front knot
        p.line(31, 106, 65, 106, top_sh, 0.8)           # hem shading
        for fx, fy in ((38, 96), (56, 100), (48, 106), (60, 94)):  # fabric pattern
            p.ell(fx, fy, fx + 2.5, fy + 2.5, tint(top, 0.35))
        # wide hanging sleeves with dark openings and small hands
        p.poly([(30, 52), (38, 52), (37, 94), (27, 90)], top, OUTLINE, 0.4)
        p.poly([(58, 52), (66, 52), (69, 90), (59, 94)], top, OUTLINE, 0.4)
        p.poly([(28, 87), (37, 91), (37, 94), (27, 90)], shade(top, 0.7))
        p.poly([(68, 87), (59, 91), (59, 94), (69, 90)], shade(top, 0.7))
        p.ell(30, 90, 36, 96, skin, OUTLINE, 0.3)
        p.ell(60, 90, 66, 96, skin, OUTLINE, 0.3)


def draw_head(p, st, emo):
    skin = SKIN_SICK if emo == "sick" else SKIN
    p.ell(HEAD["x0"], HEAD["y0"], HEAD["x1"], HEAD["y1"], skin, OUTLINE, 0.5)
    # tapered anime jaw with a soft chin point
    p.poly([(37, 32), (59, 32), (52, 46), (48, 48.5), (44, 46)], skin)
    p.chord(HEAD["x0"], HEAD["y1"] - 16, HEAD["x1"], HEAD["y1"] + 1, 25, 155,
            fade(shade(SKIN_SHADE, 0.92), 110))
    # ears peeking from under the hair
    p.ell(30.5, 24, 34, 31, skin, OUTLINE, 0.3)
    p.ell(62, 24, 65.5, 31, skin, OUTLINE, 0.3)


def draw_front_hair(p, st):
    hair, hair_dk, style = st["hair"], st["hair_dk"], st["hairstyle"]
    hair_hi = tint(hair, 0.35)
    p.chord(27, 3, 69, 46, 175, 365, hair)             # crown cap
    if style == "hime":
        p.rrect(29, 14, 67, 25, 2, hair)               # straight blunt bangs
        p.line(29, 25.5, 67, 25.5, hair_dk, 0.6)
    else:
        # jagged fringe: a zig-zag hem of pointed strands
        p.poly([(28, 14), (68, 14), (68, 21), (62.5, 27), (58, 20), (53, 26.5),
                (48, 21), (43, 26.5), (38, 20), (33.5, 26), (28, 21)], hair)
        for sx in (35.5, 45.5, 55.5, 63):              # strand separations
            p.line(sx, 15, sx - 1, 22, hair_dk, 0.5)
    if style in ("long", "hime", "fox", "unicorn"):
        # tapered locks framing the face
        p.poly([(28, 18), (34, 18), (32.5, 46), (29, 42)], hair)
        p.poly([(62, 18), (68, 18), (67, 42), (63.5, 46)], hair)
        p.line(30, 22, 29.6, 40, hair_dk, 0.5)
        p.line(66, 22, 66.4, 40, hair_dk, 0.5)
    # two-pass shine band across the crown
    p.arc(31, 5, 65, 38, 195, 255, hair_hi, 1.8)
    p.arc(35, 8, 61, 34, 200, 250, tint(hair, 0.55), 0.8)
    # soft shadow the fringe casts on the forehead (kept above the eyes)
    p.chord(30, 14, 66, 32, 0, 180, fade(shade(hair_dk, 0.7), 26))
    if style == "twintails":
        p.arc(42, 2, 56, 15, 250, 340, hair, 1.2)      # ahoge
        p.arc(43, 3, 55, 14, 255, 335, hair_hi, 0.5)
    elif style == "bob":
        p.poly([(58, 15), (66, 11), (64, 19)], st["accent"], OUTLINE, 0.3)  # hairpin
        p.line(59.5, 14, 64.5, 12, tint(st["accent"], 0.4), 0.5)
    elif style == "hime":
        for dx in (0, 3, -3):                          # small flower clip
            p.ell(60 + dx, 21, 63 + dx, 24, WHITE)
    elif style == "fox":
        p.arc(42, 4, 56, 16, 250, 340, hair, 1.2)      # ahoge
        p.arc(43, 5, 55, 15, 255, 335, hair_hi, 0.5)
    elif style == "unicorn":
        # golden horn with spiral grooves + a sparkle
        horn, horn_dk = st["accent"], (222, 188, 116)
        p.poly([(43, 12), (53, 12), (48, -4)], horn, OUTLINE, 0.4)
        p.line(44.5, 8, 51, 9.5, horn_dk, 0.6)
        p.line(45.5, 4, 50, 5.2, horn_dk, 0.6)
        p.line(57, 2, 61, 2, fade(WHITE, 200), 0.6)
        p.line(59, 0, 59, 4, fade(WHITE, 200), 0.6)


def draw_open_eye(p, st, ex, ey, droop=0.0, narrow=0.0):
    iris = st["iris"]
    h = 6.5 - int(2.5 * narrow)
    p.ell(ex - 5, ey - h, ex + 5, ey + h, WHITE, OUTLINE, 0.35)
    p.chord(ex - 5, ey - h, ex + 5, ey - h + 3.5, 180, 360, fade((150, 140, 160), 60))
    # three-tone iris: base, dark top under the lash line, lit bottom
    p.ell(ex - 3.8, ey - h + 1, ex + 3.8, ey + h - 0.5, iris)
    p.chord(ex - 3.8, ey - h + 1, ex + 3.8, ey + 1.5, 180, 360, shade(iris, 0.6))
    p.chord(ex - 3.8, ey + 0.5, ex + 3.8, ey + h - 0.5, 0, 180, tint(iris, 0.4))
    p.ell(ex - 1.7, ey - 1.2, ex + 1.7, ey + 2.6, (48, 36, 54))       # pupil
    p.ell(ex - 2.7, ey - h + 1.6, ex - 0.5, ey - h + 4.0, WHITE)      # key light
    p.ell(ex + 0.9, ey + 1.2, ex + 2.2, ey + 2.5, fade(WHITE, 220))   # rim light
    # upper lash with an outer-corner flick + a hint of lower lash
    p.arc(ex - 5.5, ey - h - 1 + droop, ex + 5.5, ey + h - 1 + droop, 190, 350, LASH, 1.5)
    p.line(ex - 5.2, ey - 2 + droop, ex - 7, ey - 3.5 + droop, LASH, 1.0)
    p.arc(ex - 3.5, ey + 1.5, ex + 3.5, ey + h + 1.5, 30, 150, tint(LASH, 0.45), 0.5)


def draw_brow(p, x0, y0, x1, y1, color, w=0.9):
    # tapered brow: a thick root stroke plus a thin tip stroke
    mx, my = (x0 + x1) / 2, (y0 + y1) / 2
    p.line(x0, y0, mx, my, color, w + 0.3)
    p.line(mx, my, x1, y1, color, max(0.4, w - 0.3))


def draw_face(p, st, emo):
    lx, rx, ey = 41, 55, 26
    brow_c = shade(st["hair_dk"], 0.85)
    nose = lambda: p.line(48, 31.5, 47.4, 33.5, shade(SKIN_SHADE, 0.82), 0.5)
    if emo == "idle":
        draw_open_eye(p, st, lx, ey)
        draw_open_eye(p, st, rx, ey)
        p.arc(lx - 5, ey - 11, lx + 5, ey - 5, 200, 340, brow_c, 0.9)
        p.arc(rx - 5, ey - 11, rx + 5, ey - 5, 200, 340, brow_c, 0.9)
        nose()
        p.arc(45, 37, 51, 42, 20, 160, MOUTH, 0.9)
        p.arc(45.5, 38.5, 50.5, 43, 30, 150, tint(MOUTH, 0.45), 0.5)
    elif emo == "happy":
        for ex in (lx, rx):
            p.arc(ex - 5.5, ey - 5, ex + 5.5, ey + 4, 190, 350, LASH, 1.6)
            p.line(ex - 5.5, ey - 2.2, ex - 7.2, ey - 3.4, LASH, 0.9)
            p.arc(ex - 5, ey - 12, ex + 5, ey - 6, 210, 330, brow_c, 0.9)
        nose()
        p.chord(43, 36, 53, 45, 0, 180, (150, 66, 70))   # open smile
        p.chord(43.8, 36, 52.2, 39.8, 0, 180, WHITE)     # teeth
        p.chord(45, 41, 51, 45, 0, 180, (235, 140, 140))  # tongue
        p.arc(43, 36, 53, 45, 0, 180, MOUTH, 0.5)
        for sx, sy in ((22, 14), (72, 12), (76, 32)):
            p.line(sx - 2.5, sy, sx + 2.5, sy, st["accent"], 0.8)
            p.line(sx, sy - 2.5, sx, sy + 2.5, st["accent"], 0.8)
    elif emo == "sad":
        draw_open_eye(p, st, lx, ey, droop=1.4)
        draw_open_eye(p, st, rx, ey, droop=1.4)
        draw_brow(p, lx - 5, ey - 8, lx + 4, ey - 10.5, brow_c)   # inner ends up
        draw_brow(p, rx + 5, ey - 8, rx - 4, ey - 10.5, brow_c)
        nose()
        p.arc(45, 40, 51, 45, 200, 340, MOUTH, 0.9)
        p.ell(rx + 5, ey + 4, rx + 8, ey + 10, (170, 210, 240), OUTLINE, 0.3)
        p.ell(rx + 5.8, ey + 4.8, rx + 7.2, ey + 6.4, fade(WHITE, 200))
        p.ell(lx - 7, ey + 8, lx - 4, ey + 13, fade((170, 210, 240), 200))
    elif emo == "angry":
        draw_open_eye(p, st, lx, ey, narrow=0.6)
        draw_open_eye(p, st, rx, ey, narrow=0.6)
        draw_brow(p, lx - 6, ey - 11, lx + 4, ey - 7, brow_c, 1.1)  # slanted in
        draw_brow(p, rx + 6, ey - 11, rx - 4, ey - 7, brow_c, 1.1)
        nose()
        p.arc(44, 40, 52, 46, 200, 340, MOUTH, 1.0)
        for a, b in (((70, 6), (76, 12)), ((76, 6), (70, 12)),
                     ((73, 4), (73, 14)), ((68, 9), (78, 9))):
            p.line(a[0], a[1], b[0], b[1], (226, 130, 120), 0.8)
    elif emo == "sleepy":
        for ex in (lx, rx):
            p.arc(ex - 5, ey - 2, ex + 5, ey + 4, 15, 165, LASH, 1.4)
            p.line(ex - 4, ey + 2.4, ex - 5.6, ey + 3.4, tint(LASH, 0.3), 0.6)
            p.arc(ex - 5, ey - 10, ex + 5, ey - 5, 210, 330, brow_c, 0.8)
        nose()
        p.ell(45.5, 38, 50.5, 43, (150, 66, 70))         # small yawn
        p.chord(45.5, 40.5, 50.5, 43, 0, 180, (235, 140, 140))
        for zx, zy, zs in ((66, 10, 4), (72, 4, 5), (79, -2, 6)):
            p.line(zx, zy, zx + zs, zy, st["accent"], 0.8)
            p.line(zx + zs, zy, zx, zy + zs, st["accent"], 0.8)
            p.line(zx, zy + zs, zx + zs, zy + zs, st["accent"], 0.8)
    elif emo == "sick":
        for ex in (lx, rx):
            p.arc(ex - 4, ey - 4, ex + 4, ey + 4, 0, 290, LASH, 0.9)
            p.arc(ex - 1.8, ey - 1.8, ex + 1.8, ey + 1.8, 100, 360, LASH, 0.9)
        p.arc(lx - 5, ey - 11, lx + 5, ey - 6, 210, 330, brow_c, 0.8)
        p.arc(rx - 5, ey - 11, rx + 5, ey - 6, 210, 330, brow_c, 0.8)
        nose()
        p.arc(44, 39, 52, 44, 190, 350, MOUTH, 0.9)      # queasy wavy mouth
        p.arc(45, 42, 51, 46, 10, 170, MOUTH, 0.9)
        p.ell(30, 6, 38, 16, (176, 216, 240), OUTLINE, 0.4)  # sweat drop
        p.poly([(34, 3), (30, 10), (38, 10)], (176, 216, 240))
        p.ell(31.8, 8, 34, 10.5, fade(WHITE, 200))
    elif emo == "dead":
        for ex in (lx, rx):
            p.line(ex - 3.5, ey - 3.5, ex + 3.5, ey + 3.5, (72, 72, 78), 1.3)
            p.line(ex - 3.5, ey + 3.5, ex + 3.5, ey - 3.5, (72, 72, 78), 1.3)
        p.line(45, 41, 51, 41, (72, 72, 78), 0.9)
    if emo not in ("dead", "sick", "angry"):
        for bx in (33, 56):
            p.ell(bx, 33, bx + 7, 38, fade(BLUSH, 85))
            p.ell(bx + 1, 34, bx + 6, 37, fade(BLUSH, 90))
    elif emo == "sick":
        for bx in (33, 56):
            p.ell(bx, 33, bx + 7, 38, fade((200, 220, 170), 80))


def draw_kolobok(p, st, emo):
    # One big golden bun: baked crust with radial shading, glossy highlight,
    # crust cracks and stubby feet. The face is drawn separately with a +28
    # y-offset so the shared emotion code lands in the bun's middle.
    bun = (216, 212, 150) if emo == "sick" else st["hair"]
    p.ell(12, 30, 84, 104, bun, OUTLINE, 0.7)
    p.chord(12, 62, 84, 106, 15, 165, st["hair_dk"])   # bottom bake shade
    p.arc(58, 36, 86, 100, 300, 60, fade(shade(st["hair_dk"], 0.85), 90), 2.0)
    p.arc(16, 36, 80, 100, 200, 250, tint(bun, 0.3), 1.8)  # top glow
    p.ell(21, 37, 41, 55, fade(tint(bun, 0.65), 110))  # soft gloss
    p.ell(24, 39, 33, 47, fade(WHITE, 150))
    for cx, cy, cl in ((30, 52, 5), (60, 44, 4), (70, 62, 4), (40, 78, 4)):
        p.arc(cx, cy, cx + cl, cy + 3, 20, 160, shade(st["hair_dk"], 0.9), 0.6)
    p.ell(30, 100, 42, 110, st["hair_dk"], OUTLINE, 0.5)   # little feet
    p.ell(54, 100, 66, 110, st["hair_dk"], OUTLINE, 0.5)
    p.arc(31, 101, 41, 108, 200, 320, tint(st["hair_dk"], 0.3), 0.6)
    p.arc(55, 101, 65, 108, 200, 320, tint(st["hair_dk"], 0.3), 0.6)


def render(char, emo):
    st = STYLE[char]
    img = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    dead = emo == "dead"
    p = Painter(d, dead=dead)
    if st["hairstyle"] == "kolobok":
        draw_kolobok(p, st, emo)
        draw_face(Painter(d, dead=dead, dy=28), st, emo)
    else:
        draw_back_hair(p, st)
        draw_body(p, st, emo)
        draw_head(p, st, emo)
        draw_front_hair(p, st)
        draw_face(p, st, emo)
    return img.resize((W, H), Image.LANCZOS)


def to_rgb565(img):
    out = bytearray()
    for r, g, b in list(img.getdata()):
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out.append(v & 0xFF)
        out.append((v >> 8) & 0xFF)
    return bytes(out)


def to_rgb565a8(img):
    # LVGL RGB565A8: full RGB565 plane first, then a byte-per-pixel A8 plane.
    color = bytearray()
    alpha = bytearray()
    for r, g, b, a in list(img.getdata()):
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        color.append(v & 0xFF)
        color.append((v >> 8) & 0xFF)
        alpha.append(a)
    return bytes(color + alpha)


# ---- battle monsters (96x96 RGB565A8, transparent) -------------------------
# Order MUST match vaiwu::game::MonsterKind: slime, bat, shroom.
MONSTERS = ["slime", "bat", "shroom", "fox", "ghost", "golem"]
MW = MH = 96


def render_monster(kind):
    img = Image.new("RGBA", (MW * S, MH * S), (0, 0, 0, 0))
    p = Painter(ImageDraw.Draw(img, "RGBA"), dead=False)
    if kind == "slime":
        body = (120, 205, 140)
        body_dk = (90, 175, 112)
        core = (150, 222, 168)
        p.ell(12, 34, 84, 92, body, OUTLINE, 0.8)          # gooey dome
        p.chord(12, 60, 84, 94, 15, 165, body_dk)          # settled bottom
        p.arc(66, 42, 86, 88, 300, 60, fade(shade(body_dk, 0.85), 90), 1.6)
        p.ell(24, 44, 72, 84, fade(core, 110))             # translucent core
        p.ell(20, 78, 34, 90, body, OUTLINE, 0.5)          # drips
        p.ell(66, 80, 78, 90, body, OUTLINE, 0.5)
        p.ell(23, 81, 31, 88, fade(core, 120))
        p.ell(40, 74, 46, 80, fade(tint(body, 0.4), 130))  # trapped bubbles
        p.ell(58, 70, 62, 74, fade(tint(body, 0.4), 130))
        p.ell(24, 40, 42, 54, fade(tint(body, 0.7), 130))  # big gloss
        p.ell(27, 42, 35, 48, fade(WHITE, 170))
        p.ell(32, 52, 46, 66, WHITE, OUTLINE, 0.4)         # eyes
        p.ell(52, 52, 66, 66, WHITE, OUTLINE, 0.4)
        p.ell(35, 55, 43, 64, (60, 130, 90))               # iris
        p.ell(55, 55, 63, 64, (60, 130, 90))
        p.ell(37.5, 58, 41.5, 63, (42, 60, 48))
        p.ell(57.5, 58, 61.5, 63, (42, 60, 48))
        p.ell(36.5, 55.5, 39, 58, WHITE)                   # eye shine
        p.ell(56.5, 55.5, 59, 58, WHITE)
        p.arc(42, 66, 56, 76, 20, 160, (52, 70, 56), 1.0)  # happy-evil grin
        p.arc(44, 66, 54, 73, 30, 150, fade(WHITE, 160), 0.5)
    elif kind == "bat":
        body = (150, 120, 200)
        body_dk = (120, 92, 170)
        wing = (100, 76, 150)
        wing_hi = (122, 96, 176)
        for pts, mirror in ((((14, 44), (2, 26), (22, 34), (30, 26), (38, 40)), 0),
                            (((82, 44), (94, 26), (74, 34), (66, 26), (58, 40)), 1)):
            p.poly(pts, wing, OUTLINE, 0.6)
        # wing membrane shading + bone struts
        p.poly([(14, 42), (8, 30), (22, 35)], wing_hi)
        p.poly([(82, 42), (88, 30), (74, 35)], wing_hi)
        for bx, by0, by1 in ((16, 30, 42), (30, 28, 40), (80, 30, 42), (66, 28, 40)):
            p.line(bx, by0, bx + (2 if bx < 48 else -2), by1, shade(wing, 0.7), 0.6)
        p.ell(28, 28, 68, 72, body, OUTLINE, 0.8)          # round body
        p.ell(36, 48, 60, 70, tint(body, 0.25))            # belly patch
        p.poly([(34, 32), (38, 16), (46, 30)], body_dk, OUTLINE, 0.5)  # ears
        p.poly([(62, 32), (58, 16), (50, 30)], body_dk, OUTLINE, 0.5)
        p.poly([(37, 29), (39, 21), (43, 29)], tint(body_dk, 0.35))    # inner ear
        p.poly([(59, 29), (57, 21), (53, 29)], tint(body_dk, 0.35))
        p.poly([(44, 44), (48, 40), (52, 44), (48, 50)], body_dk)      # chest tuft
        p.ell(35, 42, 46, 54, (250, 220, 120), OUTLINE, 0.4)  # glowing eyes
        p.ell(50, 42, 61, 54, (250, 220, 120), OUTLINE, 0.4)
        p.ell(35, 42, 46, 54, fade((250, 240, 170), 90))
        p.ell(50, 42, 61, 54, fade((250, 240, 170), 90))
        p.ell(39, 46, 43, 51, (60, 42, 80))
        p.ell(53, 46, 57, 51, (60, 42, 80))
        p.ell(37.5, 44, 40, 46.5, WHITE)
        p.ell(51.5, 44, 54, 46.5, WHITE)
        p.poly([(42, 60), (45, 67), (48, 60)], WHITE, OUTLINE, 0.2)     # fangs
        p.poly([(50, 60), (53, 67), (56, 60)], WHITE, OUTLINE, 0.2)
        p.ell(38, 70, 58, 86, body_dk, OUTLINE, 0.6)        # folded feet
        p.line(44, 74, 43, 82, shade(body_dk, 0.8), 0.5)
        p.line(52, 74, 53, 82, shade(body_dk, 0.8), 0.5)
    elif kind == "shroom":
        cap = (215, 110, 110)
        cap_dk = (185, 84, 88)
        stem = (240, 224, 200)
        p.rrect(32, 52, 64, 88, 8, stem, OUTLINE, 0.7)      # stem/face
        p.rrect(56, 54, 63, 86, 6, fade(shade(stem, 0.85), 110))  # side shade
        for gy in range(56, 72, 4):                          # gills under the cap
            p.line(34, gy, 44, gy - 2, shade(stem, 0.78), 0.5)
            p.line(62, gy, 52, gy - 2, shade(stem, 0.78), 0.5)
        p.ell(14, 20, 82, 62, cap, OUTLINE, 0.8)            # cap
        p.chord(14, 42, 82, 64, 15, 165, cap_dk)
        p.arc(18, 22, 78, 58, 190, 260, tint(cap, 0.35), 1.6)  # rim light
        for dx, dy, r in ((26, 30, 4.5), (46, 24, 5), (64, 32, 4), (36, 40, 3), (56, 42, 3.2)):
            p.ell(dx, dy, dx + 2 * r, dy + 2 * r, (245, 230, 215))
            p.ell(dx + 1, dy + 1, dx + r, dy + r, fade(WHITE, 160))
        p.arc(36, 60, 44, 70, 200, 340, (90, 62, 54), 1.0)  # grumpy brows
        p.arc(52, 60, 60, 70, 200, 340, (90, 62, 54), 1.0)
        p.ell(38, 66, 43, 72, (70, 48, 42))                 # eyes
        p.ell(53, 66, 58, 72, (70, 48, 42))
        p.ell(38.8, 67, 40.4, 68.6, fade(WHITE, 200))
        p.ell(53.8, 67, 55.4, 68.6, fade(WHITE, 200))
        p.ell(35, 72, 40, 76, fade((235, 160, 150), 90))    # flush
        p.ell(56, 72, 61, 76, fade((235, 160, 150), 90))
        p.arc(40, 76, 56, 84, 200, 340, (120, 70, 60), 1.0) # frown
    elif kind == "fox":
        fur = (216, 130, 70)
        fur_dk = (186, 104, 52)
        cream = (245, 226, 200)
        p.poly([(26, 34), (32, 10), (44, 30)], fur, OUTLINE, 0.6)   # ears
        p.poly([(70, 34), (64, 10), (52, 30)], fur, OUTLINE, 0.6)
        p.poly([(28, 22), (32, 10), (36, 22)], fur_dk)              # dark tips
        p.poly([(68, 22), (64, 10), (60, 22)], fur_dk)
        p.poly([(31, 29), (34, 17), (40, 28)], (240, 190, 190))     # inner ear
        p.poly([(65, 29), (62, 17), (56, 28)], (240, 190, 190))
        p.ell(22, 26, 74, 70, fur, OUTLINE, 0.8)                    # head
        p.chord(58, 34, 76, 66, 300, 60, fade(fur_dk, 110))         # cheek shade
        p.poly([(24, 48), (16, 52), (26, 56)], fur)                 # cheek fluff
        p.poly([(72, 48), (80, 52), (70, 56)], fur)
        p.poly([(42, 26), (48, 22), (54, 26), (48, 32)], fur_dk)    # forehead mark
        p.poly([(30, 58), (48, 84), (66, 58)], cream, OUTLINE, 0.6) # muzzle
        p.ell(44, 64, 52, 72, (70, 46, 40))                         # nose
        p.ell(45.5, 65.5, 48, 68, fade(WHITE, 170))                 # nose shine
        # sly half-lidded eyes with amber irises
        p.ell(30, 44, 44, 56, WHITE, OUTLINE, 0.3)
        p.ell(52, 44, 66, 56, WHITE, OUTLINE, 0.3)
        p.ell(33, 47, 41, 56, (226, 164, 60))
        p.ell(55, 47, 63, 56, (226, 164, 60))
        p.ell(35.5, 49, 38.5, 54, (60, 38, 32))
        p.ell(57.5, 49, 60.5, 54, (60, 38, 32))
        p.ell(34.5, 47.5, 36.5, 49.5, WHITE)
        p.ell(56.5, 47.5, 58.5, 49.5, WHITE)
        p.arc(29, 42, 45, 52, 200, 340, (60, 38, 32), 1.2)          # lids
        p.arc(51, 42, 67, 52, 200, 340, (60, 38, 32), 1.2)
        p.ell(8, 62, 30, 86, fur, OUTLINE, 0.6)                     # tail curl
        p.arc(10, 64, 28, 84, 90, 220, fur_dk, 0.8)
        p.ell(6, 66, 20, 82, cream)
        p.ell(8, 69, 16, 78, fade(WHITE, 140))
    elif kind == "ghost":
        body = (222, 226, 240)
        body_dk = (188, 194, 216)
        p.ell(24, 14, 72, 62, body, OUTLINE, 0.7)                   # round top
        p.rrect(24, 36, 72, 74, 2, body, OUTLINE, 0.7)              # sheet
        for wx in (24, 36, 48, 60):                                 # wavy hem
            p.chord(wx, 68, wx + 12, 84, 0, 180, body)
        p.chord(28, 40, 68, 76, 15, 165, body_dk)                   # inner shade
        for fx in (34, 44, 58):                                     # sheet folds
            p.arc(fx, 44, fx + 6, 78, 60, 140, fade(shade(body_dk, 0.9), 120), 0.6)
        p.arc(30, 18, 62, 50, 200, 260, tint(body, 0.5), 1.2)       # top sheen
        p.ell(34, 32, 46, 48, fade((120, 120, 170), 70))            # eye glow
        p.ell(52, 32, 64, 48, fade((120, 120, 170), 70))
        p.ell(36, 34, 46, 48, (60, 60, 84))                         # hollow eyes
        p.ell(52, 34, 62, 48, (60, 60, 84))
        p.ell(38, 36, 42, 40, (20, 20, 36))
        p.ell(54, 36, 58, 40, (20, 20, 36))
        p.ell(44, 52, 54, 62, (60, 60, 84))                         # wail mouth
        p.ell(46, 54, 52, 60, (20, 20, 36))
        p.line(14, 30, 22, 26, body_dk, 0.8)                        # wisps
        p.line(74, 28, 82, 24, body_dk, 0.8)
        p.ell(12, 46, 16, 50, fade(body, 160))                      # drifting drops
        p.ell(80, 44, 84, 48, fade(body, 160))
        p.ell(78, 60, 81, 63, fade(body, 130))
    elif kind == "golem":
        stone = (146, 148, 164)
        stone_dk = (116, 118, 136)
        stone_hi = tint(stone, 0.22)
        moss = (96, 128, 100)
        p.rrect(26, 14, 70, 50, 6, stone, OUTLINE, 0.8)             # head block
        p.rrect(28, 15, 68, 22, 4, stone_hi)                        # top bevel
        p.rrect(18, 46, 78, 88, 8, stone, OUTLINE, 0.8)             # body block
        p.rrect(20, 48, 76, 56, 6, stone_hi)                        # chest bevel
        p.rrect(8, 50, 24, 78, 5, stone_dk, OUTLINE, 0.6)           # arms
        p.rrect(72, 50, 88, 78, 5, stone_dk, OUTLINE, 0.6)
        p.rrect(7, 72, 25, 84, 4, stone, OUTLINE, 0.5)              # fists
        p.rrect(71, 72, 89, 84, 4, stone, OUTLINE, 0.5)
        p.chord(18, 66, 78, 90, 15, 165, stone_dk)                  # lower shade
        p.ell(31, 23, 47, 39, fade((250, 214, 120), 60))            # eye halo
        p.ell(49, 23, 65, 39, fade((250, 214, 120), 60))
        p.ell(34, 26, 44, 36, (250, 214, 120))                      # glowing eyes
        p.ell(52, 26, 62, 36, (250, 214, 120))
        p.ell(37, 29, 41, 33, (120, 90, 40))
        p.ell(55, 29, 59, 33, (120, 90, 40))
        p.arc(30, 20, 46, 28, 200, 340, stone_dk, 0.9)              # heavy brow
        p.arc(50, 20, 66, 28, 200, 340, stone_dk, 0.9)
        p.line(40, 44, 56, 44, stone_dk, 1.2)                       # stern mouth
        p.line(30, 56, 40, 66, stone_dk, 0.8)                       # cracks
        p.line(34, 62, 38, 58, stone_dk, 0.6)
        p.line(62, 60, 70, 70, stone_dk, 0.8)
        p.ell(24, 46, 36, 54, moss)                                 # moss patches
        p.ell(26, 48, 32, 52, tint(moss, 0.25))
        p.ell(58, 80, 70, 88, moss)
        p.ell(61, 82, 66, 86, tint(moss, 0.25))
        p.ell(60, 16, 68, 21, moss)                                 # mossy crown
    return img.resize((MW, MH), Image.LANCZOS)


# ---- home interior (320x140, opaque): where the pet lives ------------------
# The window shows the sky of the current time of day (same phases as the
# meadow); the meadow backdrops now serve as the battle scenery instead.


def render_home(phase):
    st = BG_STYLE[phase]
    img = Image.new("RGBA", (BG_W * S, BG_H * S), rgba((48, 34, 62)))
    p = Painter(ImageDraw.Draw(img), dead=False)
    # wall shading + skirting board
    p.rrect(0, 70, BG_W, 96, 0, (54, 39, 70))
    p.rrect(0, 92, BG_W, 96, 0, (40, 28, 52))
    # plank floor
    p.rrect(0, 96, BG_W, BG_H, 0, (76, 54, 74))
    for fy in (110, 124):
        p.line(0, fy, BG_W, fy, (62, 44, 62), 0.6)
    for fx in (40, 120, 200, 280):
        p.line(fx, 96, fx, BG_H, (62, 44, 62), 0.5)
    # window on the left: frame + the outside sky of this phase
    p.rrect(22, 8, 106, 66, 3, (98, 76, 106))
    p.rrect(27, 13, 101, 61, 2, st["sky0"])
    if st["moon"]:
        p.ell(78, 18, 94, 34, (244, 238, 210))   # small moon
        p.ell(83, 15, 99, 31, st["sky0"])
        for sx, sy in ((38, 22), (56, 30), (46, 44)):
            p.ell(sx, sy, sx + 2, sy + 2, (170, 158, 205))
    elif st["sun"] == "high":
        p.ell(74, 18, 92, 36, st["sun_c"])
    elif st["sun"] is not None:  # low / setting: peeking over the sill
        p.ell(40, 44, 64, 68, st["sun_c"])
    p.line(64, 13, 64, 61, (98, 76, 106), 1.0)   # window cross
    p.line(27, 37, 101, 37, (98, 76, 106), 1.0)
    # fireplace on the right, always cozy
    p.rrect(238, 42, 306, 100, 2, (64, 48, 76))
    p.rrect(246, 52, 298, 100, 2, (32, 22, 40))
    p.poly([(260, 98), (272, 70), (284, 98)], (238, 148, 84))
    p.poly([(266, 98), (272, 82), (278, 98)], (250, 196, 120))
    p.rrect(234, 38, 310, 44, 1, (98, 76, 106))  # mantel shelf
    # potted plant between window and center
    p.rrect(120, 78, 136, 94, 2, (110, 76, 84))
    p.ell(114, 58, 130, 80, (74, 110, 88))
    p.ell(126, 54, 142, 76, (64, 100, 80))
    # rug where the pet stands
    p.ell(92, 112, 228, 138, (96, 66, 92))
    p.ell(104, 116, 216, 134, (86, 60, 84))
    return img.resize((BG_W, BG_H), Image.LANCZOS).convert("RGB")


# ---- meadow backdrops (320x140, opaque): battle scenery, one per phase -----
BG_W, BG_H = 320, 140

# UI picks the phase from the wall clock (see VaiwuUi refreshScene):
# morning 5-8, day 9-16, evening 17-20, night 21-4.
BG_PHASES = ["morning", "day", "evening", "night"]

_STARS = ((26, 14, 3, (150, 138, 190)), (70, 30, 2, (120, 110, 160)),
          (120, 10, 2, (150, 138, 190)), (160, 26, 3, (200, 190, 230)),
          (205, 14, 2, (150, 138, 190)), (48, 52, 2, (120, 110, 160)),
          (232, 40, 2, (150, 138, 190)), (96, 40, 2, (120, 110, 160)))

BG_STYLE = {
    # sky0 = top of the sky, sky1/sky2 = gradient bands toward the horizon
    "morning": dict(sky0=(40, 30, 66), sky1=(54, 40, 84), sky2=(72, 52, 100),
                    hills=((46, 34, 74), (40, 30, 64)), floor=(58, 44, 92),
                    rug=((74, 58, 110), (66, 52, 100)), grass=(76, 60, 116),
                    stars=0, sun="low", sun_c=(250, 222, 170), moon=False,
                    clouds=(((170, 30),),), cloud_c=(92, 72, 118)),
    "day":     dict(sky0=(38, 50, 94), sky1=(50, 64, 112), sky2=(64, 80, 132),
                    hills=((44, 48, 86), (38, 42, 76)), floor=(54, 64, 104),
                    rug=((70, 82, 126), (62, 74, 116)), grass=(72, 86, 132),
                    stars=0, sun="high", sun_c=(252, 238, 180), moon=False,
                    clouds=(((60, 24),), ((150, 40),)), cloud_c=(86, 100, 150)),
    "evening": dict(sky0=(38, 24, 58), sky1=(64, 36, 74), sky2=(94, 50, 82),
                    hills=((36, 24, 56), (30, 20, 48)), floor=(50, 36, 80),
                    rug=((64, 48, 98), (58, 43, 90)), grass=(66, 50, 102),
                    stars=3, sun="setting", sun_c=(240, 150, 110), moon=False,
                    clouds=(((44, 34),), ((196, 50),)), cloud_c=(126, 76, 106)),
    "night":   dict(sky0=BG, sky1=(31, 24, 55), sky2=(37, 29, 66),
                    hills=((33, 25, 58), (28, 22, 50)), floor=(44, 34, 76),
                    rug=((58, 46, 96), (52, 41, 88)), grass=(60, 48, 100),
                    stars=8, sun=None, sun_c=None, moon=True,
                    clouds=(), cloud_c=None),
}


def render_bg(phase):
    st = BG_STYLE[phase]
    img = Image.new("RGBA", (BG_W * S, BG_H * S), rgba(st["sky0"]))
    p = Painter(ImageDraw.Draw(img), dead=False)
    # sky gradient bands toward the horizon
    p.rrect(0, 46, BG_W, 80, 0, st["sky1"])
    p.rrect(0, 80, BG_W, 106, 0, st["sky2"])
    # stars (night: all; evening: the first few)
    for sx, sy, l, c in _STARS[:st["stars"]]:
        p.line(sx - l, sy, sx + l, sy, c, 0.6)
        p.line(sx, sy - l, sx, sy + l, c, 0.6)
    # sun (drawn before the hills so a low sun peeks from behind them)
    if st["sun"] == "high":
        p.ell(238, 12, 274, 48, st["sun_c"])
        p.arc(232, 6, 280, 54, 0, 360, tuple(min(255, v + 12) for v in st["sun_c"]), 0.6)
    elif st["sun"] == "low":
        p.ell(34, 72, 74, 112, st["sun_c"])
    elif st["sun"] == "setting":
        p.ell(92, 82, 136, 126, st["sun_c"])
    # clouds: a puff of three overlapping ellipses per position
    for cloud in st["clouds"]:
        for cx, cy in cloud:
            p.ell(cx - 16, cy - 5, cx + 4, cy + 6, st["cloud_c"])
            p.ell(cx - 6, cy - 9, cx + 14, cy + 4, st["cloud_c"])
            p.ell(cx + 2, cy - 4, cx + 22, cy + 6, st["cloud_c"])
    # crescent moon (second ellipse in sky color carves the bite)
    if st["moon"]:
        p.ell(262, 10, 298, 46, (244, 238, 210))
        p.ell(272, 4, 308, 40, st["sky0"])
    # rolling hill silhouettes on the horizon
    p.ell(-40, 86, 150, 156, st["hills"][0])
    p.ell(110, 92, 330, 160, st["hills"][1])
    # meadow floor + a round rug right where the pet stands
    p.rrect(0, 106, BG_W, BG_H, 0, st["floor"])
    p.ell(92, 112, 228, 138, st["rug"][0])
    p.ell(104, 116, 216, 134, st["rug"][1])
    # a few tufts of grass at the floor edges
    for gx in (16, 40, 268, 296):
        p.line(gx, 112, gx - 2, 104, st["grass"], 0.6)
        p.line(gx, 112, gx + 2, 105, st["grass"], 0.6)
    return img.resize((BG_W, BG_H), Image.LANCZOS).convert("RGB")


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    assets = os.path.join(root, "assets")
    os.makedirs(assets, exist_ok=True)
    imgs = {}
    for char in CHARS:
        for emo in EMOTIONS:
            img = render(char, emo)
            imgs[(char, emo)] = img
            img.save(os.path.join(assets, f"sprite_{char}_{emo}.png"))
    mon_imgs = {}
    for kind in MONSTERS:
        mon_imgs[kind] = render_monster(kind)
        mon_imgs[kind].save(os.path.join(assets, f"monster_{kind}.png"))
    mpad = 8
    mon_grid = Image.new("RGB", (mpad + len(MONSTERS) * (MW + mpad), MH + 2 * mpad), BG)
    for i, kind in enumerate(MONSTERS):
        mon_grid.paste(mon_imgs[kind], (mpad + i * (MW + mpad), mpad), mon_imgs[kind])
    mon_grid.resize((mon_grid.width * 2, mon_grid.height * 2), Image.NEAREST).save(
        os.path.join(assets, "monsters_preview.png"))

    bg_imgs = {}
    home_imgs = {}
    for phase in BG_PHASES:
        bg_imgs[phase] = render_bg(phase)
        bg_imgs[phase].save(os.path.join(assets, f"bg_{phase}.png"))
        home_imgs[phase] = render_home(phase)
        home_imgs[phase].save(os.path.join(assets, f"home_{phase}.png"))
    pad = 6
    bg_grid = Image.new("RGB", (pad + 2 * (BG_W + pad), pad + 2 * (BG_H + pad)), BG)
    home_grid = Image.new("RGB", (pad + 2 * (BG_W + pad), pad + 2 * (BG_H + pad)), BG)
    for i, phase in enumerate(BG_PHASES):
        bg_grid.paste(bg_imgs[phase], (pad + (i % 2) * (BG_W + pad), pad + (i // 2) * (BG_H + pad)))
        home_grid.paste(home_imgs[phase], (pad + (i % 2) * (BG_W + pad), pad + (i // 2) * (BG_H + pad)))
    bg_grid.save(os.path.join(assets, "bg_preview.png"))
    home_grid.save(os.path.join(assets, "home_preview.png"))
    pad = 8
    grid = Image.new("RGB", (pad + len(EMOTIONS) * (W + pad), pad + len(CHARS) * (H + pad)), BG)
    for r, char in enumerate(CHARS):
        for cc, emo in enumerate(EMOTIONS):
            grid.paste(imgs[(char, emo)], (pad + cc * (W + pad), pad + r * (H + pad)),
                       imgs[(char, emo)])
    grid = grid.resize((grid.width * 2, grid.height * 2), Image.NEAREST)
    grid.save(os.path.join(assets, "sprites_preview.png"))
    c_path = os.path.join(root, "components/ui/src/sprites.c")
    h_path = os.path.join(root, "components/ui/include/vaiwu/ui/sprites.h")
    with open(c_path, "w") as c:
        c.write("/* Auto-generated by tools/gen_sprites.py - do not edit by hand. */\n")
        c.write('#include "vaiwu/ui/sprites.h"\n\n')
        for char in CHARS:
            for emo in EMOTIONS:
                data = to_rgb565a8(imgs[(char, emo)])
                name = f"{char}_{emo}"
                c.write(f"static const uint8_t sprite_{name}_map[{len(data)}] = {{\n")
                for i in range(0, len(data), 24):
                    c.write("    " + ",".join(str(b) for b in data[i:i + 24]) + ",\n")
                c.write("};\n\n")
                c.write(f"const lv_image_dsc_t vaiwu_sprite_{name} = {{\n")
                c.write("    .header = {\n        .magic = LV_IMAGE_HEADER_MAGIC,\n")
                c.write("        .cf = LV_COLOR_FORMAT_RGB565A8,\n")
                c.write(f"        .w = {W},\n        .h = {H},\n        .stride = {W * 2},\n    }},\n")
                c.write(f"    .data_size = {len(data)},\n    .data = sprite_{name}_map,\n}};\n\n")
        for phase in BG_PHASES:
            bg_data = to_rgb565(bg_imgs[phase])
            c.write(f"static const uint8_t bg_{phase}_map[{len(bg_data)}] = {{\n")
            for i in range(0, len(bg_data), 24):
                c.write("    " + ",".join(str(b) for b in bg_data[i:i + 24]) + ",\n")
            c.write("};\n\n")
            c.write(f"const lv_image_dsc_t vaiwu_bg_{phase} = {{\n")
            c.write("    .header = {\n        .magic = LV_IMAGE_HEADER_MAGIC,\n")
            c.write("        .cf = LV_COLOR_FORMAT_RGB565,\n")
            c.write(f"        .w = {BG_W},\n        .h = {BG_H},\n        .stride = {BG_W * 2},\n    }},\n")
            c.write(f"    .data_size = {len(bg_data)},\n    .data = bg_{phase}_map,\n}};\n\n")
        for kind in MONSTERS:
            data = to_rgb565a8(mon_imgs[kind])
            c.write(f"static const uint8_t monster_{kind}_map[{len(data)}] = {{\n")
            for i in range(0, len(data), 24):
                c.write("    " + ",".join(str(b) for b in data[i:i + 24]) + ",\n")
            c.write("};\n\n")
            c.write(f"const lv_image_dsc_t vaiwu_monster_{kind} = {{\n")
            c.write("    .header = {\n        .magic = LV_IMAGE_HEADER_MAGIC,\n")
            c.write("        .cf = LV_COLOR_FORMAT_RGB565A8,\n")
            c.write(f"        .w = {MW},\n        .h = {MH},\n        .stride = {MW * 2},\n    }},\n")
            c.write(f"    .data_size = {len(data)},\n    .data = monster_{kind}_map,\n}};\n\n")
        c.write("/* Battle monsters, order matches vaiwu::game::MonsterKind. */\n")
        c.write(f"const lv_image_dsc_t* const vaiwu_monsters[{len(MONSTERS)}] = {{\n")
        c.write("    " + ", ".join(f"&vaiwu_monster_{k}" for k in MONSTERS) + ",\n};\n\n")
        c.write("/* Time-of-day order: morning, day, evening, night. */\n")
        c.write(f"const lv_image_dsc_t* const vaiwu_bgs[{len(BG_PHASES)}] = {{\n")
        c.write("    " + ", ".join(f"&vaiwu_bg_{ph}" for ph in BG_PHASES) + ",\n};\n\n")
        for phase in BG_PHASES:
            data = to_rgb565(home_imgs[phase])
            c.write(f"static const uint8_t home_{phase}_map[{len(data)}] = {{\n")
            for i in range(0, len(data), 24):
                c.write("    " + ",".join(str(b) for b in data[i:i + 24]) + ",\n")
            c.write("};\n\n")
            c.write(f"const lv_image_dsc_t vaiwu_home_{phase} = {{\n")
            c.write("    .header = {\n        .magic = LV_IMAGE_HEADER_MAGIC,\n")
            c.write("        .cf = LV_COLOR_FORMAT_RGB565,\n")
            c.write(f"        .w = {BG_W},\n        .h = {BG_H},\n        .stride = {BG_W * 2},\n    }},\n")
            c.write(f"    .data_size = {len(data)},\n    .data = home_{phase}_map,\n}};\n\n")
        c.write("/* Home interior, same phase order (window sky follows the clock). */\n")
        c.write(f"const lv_image_dsc_t* const vaiwu_homes[{len(BG_PHASES)}] = {{\n")
        c.write("    " + ", ".join(f"&vaiwu_home_{ph}" for ph in BG_PHASES) + ",\n};\n\n")
        c.write("/* [character][emotion] lookup, order matches vaiwu::game::Emotion. */\n")
        c.write(f"const lv_image_dsc_t* const vaiwu_sprites[{len(CHARS)}][{len(EMOTIONS)}] = {{\n")
        for char in CHARS:
            row = ", ".join(f"&vaiwu_sprite_{char}_{emo}" for emo in EMOTIONS)
            c.write(f"    {{{row}}},\n")
        c.write("};\n")
    with open(h_path, "w") as h:
        h.write("/* Auto-generated by tools/gen_sprites.py - do not edit by hand. */\n")
        h.write("#pragma once\n\n#include \"lvgl.h\"\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
        for char in CHARS:
            for emo in EMOTIONS:
                h.write(f"extern const lv_image_dsc_t vaiwu_sprite_{char}_{emo};\n")
        h.write(f"\nextern const lv_image_dsc_t* const vaiwu_sprites[{len(CHARS)}][{len(EMOTIONS)}];\n")
        h.write("\n/* Main-screen scene backdrops (320x140, opaque), one per time of\n")
        h.write(" * day: morning, day, evening, night (see vaiwu_bgs order). */\n")
        for phase in BG_PHASES:
            h.write(f"extern const lv_image_dsc_t vaiwu_bg_{phase};\n")
        h.write(f"extern const lv_image_dsc_t* const vaiwu_bgs[{len(BG_PHASES)}];\n")
        h.write("\n/* Home interior (main screen), same phase order. */\n")
        for phase in BG_PHASES:
            h.write(f"extern const lv_image_dsc_t vaiwu_home_{phase};\n")
        h.write(f"extern const lv_image_dsc_t* const vaiwu_homes[{len(BG_PHASES)}];\n")
        h.write("\n/* Battle monsters (96x96, transparent), order = MonsterKind. */\n")
        for kind in MONSTERS:
            h.write(f"extern const lv_image_dsc_t vaiwu_monster_{kind};\n")
        h.write(f"extern const lv_image_dsc_t* const vaiwu_monsters[{len(MONSTERS)}];\n")
        h.write("\n#ifdef __cplusplus\n}\n#endif\n")
    print(f"generated {len(CHARS)}x{len(EMOTIONS)} sprites")


if __name__ == "__main__":
    main()
