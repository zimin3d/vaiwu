#!/usr/bin/env python3
"""Generate the waifu sprite set (7 chars x 7 emotions, 96x128 RGB565A8 with a
transparent background) plus the main-screen scene backdrop (320x140 RGB565).

Style: a more grown-up anime look (taller, less chibi proportions) in a soft
pastel "caramel" palette - warm creamy skin, muted pastel outfits, distinct
hairstyles and clothes per character. PNGs in assets/ are PREVIEWS ONLY; the
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
MOUTH = (196, 110, 112)
LASH = (86, 60, 70)

EMOTIONS = ["idle", "happy", "sad", "angry", "sleepy", "sick", "dead"]
CHARS = ["aoi", "hina", "rin", "yuki", "akane", "unicorn", "kolobok"]

STYLE = {
    "aoi":  dict(hair=(150, 186, 224), hair_dk=(120, 158, 200), iris=(120, 170, 214),
                 top=(196, 216, 236), bottom=(170, 196, 224), accent=(224, 236, 248),
                 hairstyle="long", outfit="sundress"),
    "hina": dict(hair=(240, 190, 150), hair_dk=(216, 160, 116), iris=(226, 176, 116),
                 top=(246, 214, 186), bottom=(238, 196, 160), accent=(252, 232, 210),
                 hairstyle="twintails", outfit="hoodie"),
    "rin":  dict(hair=(232, 168, 176), hair_dk=(206, 138, 150), iris=(214, 138, 150),
                 top=(244, 210, 216), bottom=(150, 120, 150), accent=(250, 230, 234),
                 hairstyle="bob", outfit="sailor"),
    "yuki": dict(hair=(206, 194, 232), hair_dk=(178, 164, 212), iris=(170, 150, 214),
                 top=(224, 214, 240), bottom=(206, 194, 230), accent=(238, 230, 248),
                 hairstyle="hime", outfit="kimono"),
    "akane": dict(hair=(238, 186, 150), hair_dk=(212, 154, 110), iris=(226, 178, 120),
                  top=(240, 206, 176), bottom=(214, 176, 140), accent=(250, 234, 214),
                  hairstyle="fox", outfit="foxhoodie"),
    # Unicorn girl: pastel lilac mane + golden horn (see draw_front_hair).
    "unicorn": dict(hair=(224, 186, 242), hair_dk=(198, 156, 226), iris=(172, 142, 230),
                    top=(236, 222, 248), bottom=(210, 190, 238), accent=(248, 226, 170),
                    hairstyle="unicorn", outfit="sundress"),
    # Kolobok: not humanoid - a round golden bread bun with a simple face
    # (special-cased in render()). hair=bun crust, hair_dk=crust shading.
    "kolobok": dict(hair=(242, 206, 130), hair_dk=(212, 168, 88), iris=(150, 110, 60),
                    top=(242, 206, 130), bottom=(212, 168, 88), accent=(252, 238, 200),
                    hairstyle="kolobok", outfit="none"),
}


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


HEAD = dict(x0=31, y0=8, x1=65, y1=52)


def draw_back_hair(p, st):
    hair, hair_dk, style = st["hair"], st["hair_dk"], st["hairstyle"]
    if style == "long":
        p.ell(24, 12, 72, 58, hair_dk)
        p.rrect(26, 40, 70, 104, 10, hair_dk)
        p.ell(24, 92, 46, 108, hair_dk)
        p.ell(50, 92, 72, 108, hair_dk)
    elif style == "twintails":
        p.ell(26, 12, 70, 56, hair_dk)
        # high side ponytails: small puffs at the temples, slim tails hanging
        # straight down close to the body (so they don't read as arms).
        p.ell(20, 14, 34, 30, hair_dk)                 # left puff (high)
        p.ell(62, 14, 76, 30, hair_dk)                 # right puff (high)
        p.rrect(16, 26, 26, 86, 6, hair_dk)            # left tail
        p.rrect(70, 26, 80, 86, 6, hair_dk)            # right tail
        p.ell(15, 80, 27, 94, hair_dk)                 # tail tips
        p.ell(69, 80, 81, 94, hair_dk)
        # little bows where the ties sit
        p.ell(19, 18, 27, 26, st["accent"], OUTLINE, 0.4)
        p.ell(69, 18, 77, 26, st["accent"], OUTLINE, 0.4)
    elif style == "bob":
        p.ell(26, 12, 70, 60, hair_dk)
        p.rrect(24, 34, 72, 62, 10, hair_dk)
    elif style == "hime":
        p.ell(24, 10, 72, 56, hair_dk)
        p.rrect(24, 36, 72, 108, 10, hair_dk)
        p.rrect(24, 36, 34, 78, 4, hair_dk)
        p.rrect(62, 36, 72, 78, 4, hair_dk)
    elif style == "fox":
        p.ell(26, 12, 70, 58, hair_dk)
        p.rrect(24, 38, 70, 100, 10, hair_dk)
        p.ell(62, 82, 92, 118, hair_dk)
        p.ell(72, 98, 90, 116, WHITE)
        p.poly([(24, 22), (33, -2), (44, 26)], hair)
        p.poly([(52, 26), (63, -2), (72, 22)], hair)
        p.poly([(29, 20), (34, 6), (40, 22)], st["accent"])
        p.poly([(56, 22), (62, 6), (67, 20)], st["accent"])
    elif style == "unicorn":
        # long flowing mane sweeping outwards, with lighter pastel streaks
        p.ell(24, 12, 72, 58, hair_dk)
        p.rrect(26, 40, 72, 102, 10, hair_dk)
        p.ell(58, 88, 84, 112, hair_dk)          # mane tip curling out
        p.ell(24, 92, 46, 108, hair_dk)
        streak = tuple(min(255, v + 24) for v in hair_dk)
        p.rrect(32, 44, 38, 96, 3, streak)       # pastel streaks in the mane
        p.rrect(58, 46, 64, 100, 3, streak)
        p.rrect(66, 60, 71, 98, 2, st["top"])


def draw_body(p, st, emo):
    skin = SKIN_SICK if emo == "sick" else SKIN
    top, bottom, accent, outfit = st["top"], st["bottom"], st["accent"], st["outfit"]

    p.rrect(44, 48, 52, 58, 3, skin)
    p.rrect(40, 100, 47, 124, 3, skin, OUTLINE, 0.5)
    p.rrect(49, 100, 56, 124, 3, skin, OUTLINE, 0.5)
    p.rrect(38, 120, 48, 126, 2, accent, OUTLINE, 0.5)
    p.rrect(48, 120, 58, 126, 2, accent, OUTLINE, 0.5)

    if outfit == "sundress":
        p.poly([(38, 56), (58, 56), (68, 104), (28, 104)], top, OUTLINE, 0.7)
        p.rrect(30, 100, 66, 108, 4, bottom)
        p.line(42, 50, 44, 58, accent, 1.0)
        p.line(54, 50, 52, 58, accent, 1.0)
    elif outfit == "hoodie":
        p.rrect(32, 56, 64, 98, 8, top, OUTLINE, 0.7)
        p.rrect(30, 92, 66, 116, 6, bottom, OUTLINE, 0.6)
        p.arc(40, 52, 56, 66, 20, 160, accent, 1.4)
        p.line(48, 60, 48, 84, st["hair_dk"], 0.8)
        p.ell(44, 84, 47, 88, accent)
        p.ell(49, 84, 52, 88, accent)
    elif outfit == "sailor":
        p.rrect(34, 56, 62, 92, 6, top, OUTLINE, 0.7)
        p.poly([(40, 56), (56, 56), (48, 74)], accent)
        p.poly([(45, 66), (51, 66), (48, 76)], bottom)
        p.rrect(30, 90, 66, 112, 4, bottom, OUTLINE, 0.6)
        for sx in range(34, 64, 6):
            p.line(sx, 92, sx, 110, top, 0.6)
    elif outfit == "kimono":
        p.poly([(36, 56), (60, 56), (66, 110), (30, 110)], top, OUTLINE, 0.7)
        p.poly([(48, 56), (60, 56), (48, 84)], accent)
        p.poly([(48, 56), (36, 56), (48, 84)], bottom)
        p.rrect(34, 78, 62, 88, 2, bottom)
        p.rrect(44, 78, 52, 88, 2, accent)
    elif outfit == "foxhoodie":
        p.rrect(32, 56, 64, 100, 8, top, OUTLINE, 0.7)
        p.rrect(30, 94, 66, 116, 6, bottom, OUTLINE, 0.6)
        p.arc(40, 52, 56, 66, 20, 160, accent, 1.4)
        p.ell(43, 84, 53, 92, accent)
        p.line(48, 60, 48, 82, st["hair_dk"], 0.8)

    p.rrect(28, 60, 35, 96, 3, skin, OUTLINE, 0.5)
    p.rrect(61, 60, 68, 96, 3, skin, OUTLINE, 0.5)


def draw_head(p, st, emo):
    skin = SKIN_SICK if emo == "sick" else SKIN
    p.ell(HEAD["x0"], HEAD["y0"], HEAD["x1"], HEAD["y1"], skin, OUTLINE, 0.7)
    p.chord(HEAD["x0"], HEAD["y1"] - 20, HEAD["x1"], HEAD["y1"] + 2, 20, 160,
            SKIN_SICK if emo == "sick" else SKIN_SHADE)


def draw_front_hair(p, st):
    hair, hair_dk, style = st["hair"], st["hair_dk"], st["hairstyle"]
    p.chord(28, 4, 68, 50, 178, 362, hair)
    for x0, x1, drop in [(29, 41, 30), (39, 51, 33), (47, 59, 33), (57, 67, 30)]:
        p.chord(x0, 14, x1, drop, 0, 180, hair)
    if style in ("long", "hime", "fox", "unicorn"):
        p.rrect(28, 22, 33, 46, 3, hair)
        p.rrect(63, 22, 68, 46, 3, hair)
    p.arc(32, 8, 64, 40, 200, 250, tuple(min(255, v + 30) for v in hair), 1.6)
    if style == "twintails":
        p.arc(42, 2, 56, 16, 250, 340, hair, 1.2)      # ahoge
    elif style == "bob":
        p.poly([(58, 16), (66, 12), (64, 20)], st["accent"])
    elif style == "hime":
        for dx in (0, 3, -3):
            p.ell(60 + dx, 22, 63 + dx, 25, WHITE)
    elif style == "fox":
        p.arc(42, 4, 56, 16, 250, 340, hair, 1.2)
    elif style == "unicorn":
        # golden horn with a couple of spiral grooves + a sparkle
        horn, horn_dk = st["accent"], (222, 188, 116)
        p.poly([(43, 12), (53, 12), (48, -4)], horn, OUTLINE, 0.4)
        p.line(44.5, 8, 51, 9.5, horn_dk, 0.6)
        p.line(45.5, 4, 50, 5.2, horn_dk, 0.6)
        p.line(57, 2, 61, 2, WHITE, 0.6)
        p.line(59, 0, 59, 4, WHITE, 0.6)


def draw_open_eye(p, st, ex, ey, droop=0.0, narrow=0.0):
    iris = st["iris"]
    h = 6 - int(2.5 * narrow)
    p.ell(ex - 4.5, ey - h, ex + 4.5, ey + h, WHITE, OUTLINE, 0.4)
    p.ell(ex - 3.4, ey - h + 1.2, ex + 3.4, ey + h - 0.6, iris)
    p.ell(ex - 1.6, ey - 0.5, ex + 1.6, ey + h - 2.0, (54, 42, 58))
    p.ell(ex - 2.6, ey - h + 1.6, ex - 0.4, ey - h + 3.8, WHITE)
    p.ell(ex + 0.9, ey + 1.2, ex + 2.0, ey + 2.6, WHITE)
    p.arc(ex - 5.5, ey - h - 1 + droop, ex + 5.5, ey + h - 1 + droop, 195, 345, LASH, 1.2)
    p.line(ex - 5.5, ey - 2 + droop, ex - 3.2, ey - 3 + droop, LASH, 1.0)


def draw_face(p, st, emo):
    lx, rx, ey = 40, 56, 30
    if emo == "idle":
        draw_open_eye(p, st, lx, ey)
        draw_open_eye(p, st, rx, ey)
        p.arc(45, 38, 51, 43, 20, 160, MOUTH, 0.9)
    elif emo == "happy":
        p.arc(lx - 5, ey - 4, lx + 5, ey + 5, 190, 350, LASH, 1.5)
        p.arc(rx - 5, ey - 4, rx + 5, ey + 5, 190, 350, LASH, 1.5)
        p.chord(44, 37, 52, 45, 0, 180, MOUTH)
        p.chord(45, 37, 51, 41, 0, 180, WHITE)
        for sx, sy in ((22, 16), (72, 14), (76, 34)):
            p.line(sx - 2.5, sy, sx + 2.5, sy, st["accent"], 0.8)
            p.line(sx, sy - 2.5, sx, sy + 2.5, st["accent"], 0.8)
    elif emo == "sad":
        draw_open_eye(p, st, lx, ey, droop=1.4)
        draw_open_eye(p, st, rx, ey, droop=1.4)
        p.line(lx - 5, ey - 7, lx + 4, ey - 5, LASH, 0.9)
        p.line(rx + 5, ey - 7, rx - 4, ey - 5, LASH, 0.9)
        p.arc(45, 40, 51, 45, 200, 340, MOUTH, 0.9)
        p.ell(rx + 5, ey + 5, rx + 8, ey + 11, (170, 210, 240), OUTLINE, 0.3)
    elif emo == "angry":
        draw_open_eye(p, st, lx, ey, narrow=0.5)
        draw_open_eye(p, st, rx, ey, narrow=0.5)
        p.line(lx - 6, ey - 9, lx + 4, ey - 6, LASH, 1.2)
        p.line(rx + 6, ey - 9, rx - 4, ey - 6, LASH, 1.2)
        p.arc(44, 40, 52, 46, 200, 340, MOUTH, 1.0)
        for a, b in (((70, 8), (76, 14)), ((76, 8), (70, 14)),
                     ((73, 6), (73, 16)), ((68, 11), (78, 11))):
            p.line(a[0], a[1], b[0], b[1], (226, 130, 120), 0.8)
    elif emo == "sleepy":
        p.arc(lx - 5, ey - 2, lx + 5, ey + 4, 15, 165, LASH, 1.4)
        p.arc(rx - 5, ey - 2, rx + 5, ey + 4, 15, 165, LASH, 1.4)
        p.arc(46, 39, 50, 43, 20, 160, MOUTH, 0.8)
        for zx, zy, zs in ((66, 12, 4), (72, 6, 5), (79, 0, 6)):
            p.line(zx, zy, zx + zs, zy, st["accent"], 0.8)
            p.line(zx + zs, zy, zx, zy + zs, st["accent"], 0.8)
            p.line(zx, zy + zs, zx + zs, zy + zs, st["accent"], 0.8)
    elif emo == "sick":
        for ex in (lx, rx):
            p.arc(ex - 4, ey - 4, ex + 4, ey + 4, 0, 290, LASH, 0.9)
            p.arc(ex - 1.8, ey - 1.8, ex + 1.8, ey + 1.8, 100, 360, LASH, 0.9)
        p.arc(44, 39, 52, 44, 190, 350, MOUTH, 0.9)
        p.arc(45, 42, 51, 46, 10, 170, MOUTH, 0.9)
        p.ell(30, 6, 38, 16, (176, 216, 240), OUTLINE, 0.4)
        p.poly([(34, 3), (30, 10), (38, 10)], (176, 216, 240))
    elif emo == "dead":
        for ex in (lx, rx):
            p.line(ex - 3.5, ey - 3.5, ex + 3.5, ey + 3.5, (72, 72, 78), 1.3)
            p.line(ex - 3.5, ey + 3.5, ex + 3.5, ey - 3.5, (72, 72, 78), 1.3)
        p.line(45, 41, 51, 41, (72, 72, 78), 0.9)
    if emo not in ("dead", "sick", "angry"):
        p.ell(33, 33, 40, 38, BLUSH)
        p.ell(56, 33, 63, 38, BLUSH)


def draw_kolobok(p, st, emo):
    # One big golden bun instead of a body: crust, bottom shading, a shiny
    # highlight and a few crust cracks. The face is drawn separately with a
    # +28 y-offset so the shared emotion code lands in the bun's middle.
    bun = (216, 212, 150) if emo == "sick" else st["hair"]
    p.ell(12, 32, 84, 104, bun, OUTLINE, 0.8)
    p.chord(12, 66, 84, 106, 20, 160, st["hair_dk"])
    p.arc(18, 38, 78, 98, 200, 250, tuple(min(255, v + 28) for v in bun), 1.8)
    for cx, cy, cl in ((30, 44, 5), (62, 40, 4), (70, 60, 4)):  # crust cracks
        p.arc(cx, cy, cx + cl, cy + 3, 20, 160, st["hair_dk"], 0.6)
    # stubby little feet so he can "run away from grandma"
    p.ell(30, 100, 42, 110, st["hair_dk"], OUTLINE, 0.5)
    p.ell(54, 100, 66, 110, st["hair_dk"], OUTLINE, 0.5)


def render(char, emo):
    st = STYLE[char]
    img = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
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
    p = Painter(ImageDraw.Draw(img), dead=False)
    if kind == "slime":
        body = (120, 205, 140)
        body_dk = (90, 175, 112)
        p.ell(12, 34, 84, 92, body, OUTLINE, 0.8)          # gooey dome
        p.chord(12, 62, 84, 94, 15, 165, body_dk)
        p.ell(20, 78, 34, 90, body, OUTLINE, 0.5)          # drips
        p.ell(66, 80, 78, 90, body, OUTLINE, 0.5)
        p.ell(26, 42, 40, 52, (210, 245, 215))             # shine
        p.ell(34, 54, 44, 66, WHITE, OUTLINE, 0.4)         # eyes
        p.ell(54, 54, 64, 66, WHITE, OUTLINE, 0.4)
        p.ell(37.5, 58, 42.5, 64, (52, 70, 56))
        p.ell(56.5, 58, 61.5, 64, (52, 70, 56))
        p.arc(42, 66, 56, 76, 20, 160, (52, 70, 56), 1.0)  # happy-evil grin
    elif kind == "bat":
        body = (150, 120, 200)
        body_dk = (120, 92, 170)
        wing = (100, 76, 150)
        p.poly([(14, 44), (2, 26), (22, 34), (30, 26), (38, 40)], wing, OUTLINE, 0.6)   # left wing
        p.poly([(82, 44), (94, 26), (74, 34), (66, 26), (58, 40)], wing, OUTLINE, 0.6)  # right wing
        p.ell(28, 28, 68, 72, body, OUTLINE, 0.8)          # round body
        p.poly([(34, 32), (38, 16), (46, 30)], body_dk, OUTLINE, 0.5)  # ears
        p.poly([(62, 32), (58, 16), (50, 30)], body_dk, OUTLINE, 0.5)
        p.ell(36, 42, 46, 54, (250, 220, 120), OUTLINE, 0.4)  # glowing eyes
        p.ell(52, 42, 62, 54, (250, 220, 120), OUTLINE, 0.4)
        p.ell(39.5, 46, 43.5, 51, (60, 42, 80))
        p.ell(54.5, 46, 58.5, 51, (60, 42, 80))
        p.poly([(42, 60), (45, 66), (48, 60)], WHITE)       # fangs
        p.poly([(50, 60), (53, 66), (56, 60)], WHITE)
        p.ell(38, 70, 58, 86, body_dk, OUTLINE, 0.6)        # folded feet
    elif kind == "shroom":
        cap = (215, 110, 110)
        cap_dk = (185, 84, 88)
        stem = (240, 224, 200)
        p.rrect(32, 52, 64, 88, 8, stem, OUTLINE, 0.7)      # stem/face
        p.ell(14, 20, 82, 62, cap, OUTLINE, 0.8)            # cap
        p.chord(14, 44, 82, 64, 15, 165, cap_dk)
        for dx, dy in ((26, 30), (46, 24), (64, 32)):       # cap dots
            p.ell(dx, dy, dx + 9, dy + 9, (245, 230, 215))
        p.arc(36, 60, 44, 70, 200, 340, (90, 62, 54), 1.0)  # grumpy brows
        p.arc(52, 60, 60, 70, 200, 340, (90, 62, 54), 1.0)
        p.ell(38, 66, 43, 72, (70, 48, 42))                 # eyes
        p.ell(53, 66, 58, 72, (70, 48, 42))
        p.arc(40, 76, 56, 84, 200, 340, (120, 70, 60), 1.0) # frown
    elif kind == "fox":
        fur = (216, 130, 70)
        fur_dk = (186, 104, 52)
        cream = (245, 226, 200)
        p.poly([(26, 34), (32, 10), (44, 30)], fur, OUTLINE, 0.6)   # ears
        p.poly([(70, 34), (64, 10), (52, 30)], fur, OUTLINE, 0.6)
        p.poly([(30, 30), (34, 16), (41, 28)], fur_dk)
        p.poly([(66, 30), (62, 16), (55, 28)], fur_dk)
        p.ell(22, 26, 74, 70, fur, OUTLINE, 0.8)                    # head
        p.poly([(30, 58), (48, 84), (66, 58)], cream, OUTLINE, 0.6) # muzzle
        p.ell(44, 64, 52, 72, (70, 46, 40))                         # nose
        p.arc(30, 44, 44, 54, 200, 340, (60, 38, 32), 1.2)          # sly eyes
        p.arc(52, 44, 66, 54, 200, 340, (60, 38, 32), 1.2)
        p.ell(8, 62, 30, 86, fur, OUTLINE, 0.6)                     # tail curl
        p.ell(6, 66, 20, 82, cream)
    elif kind == "ghost":
        body = (222, 226, 240)
        body_dk = (188, 194, 216)
        p.ell(24, 14, 72, 62, body, OUTLINE, 0.7)                   # round top
        p.rrect(24, 36, 72, 74, 2, body, OUTLINE, 0.7)              # sheet
        for wx in (24, 36, 48, 60):                                 # wavy hem
            p.chord(wx, 68, wx + 12, 84, 0, 180, body)
        p.chord(28, 40, 68, 76, 15, 165, body_dk)                   # inner shade
        p.ell(36, 34, 46, 48, (60, 60, 84))                         # hollow eyes
        p.ell(52, 34, 62, 48, (60, 60, 84))
        p.ell(44, 52, 54, 62, (60, 60, 84))                         # wail mouth
        p.line(14, 30, 22, 26, body_dk, 0.8)                        # wisps
        p.line(74, 28, 82, 24, body_dk, 0.8)
    elif kind == "golem":
        stone = (146, 148, 164)
        stone_dk = (116, 118, 136)
        moss = (96, 128, 100)
        p.rrect(26, 14, 70, 50, 6, stone, OUTLINE, 0.8)             # head block
        p.rrect(18, 46, 78, 88, 8, stone, OUTLINE, 0.8)             # body block
        p.rrect(8, 50, 24, 78, 5, stone_dk, OUTLINE, 0.6)           # arms
        p.rrect(72, 50, 88, 78, 5, stone_dk, OUTLINE, 0.6)
        p.chord(18, 66, 78, 90, 15, 165, stone_dk)                  # lower shade
        p.ell(34, 26, 44, 36, (250, 214, 120))                      # glowing eyes
        p.ell(52, 26, 62, 36, (250, 214, 120))
        p.line(40, 44, 56, 44, stone_dk, 1.2)                       # stern mouth
        p.line(30, 56, 40, 66, stone_dk, 0.8)                       # cracks
        p.line(62, 60, 70, 70, stone_dk, 0.8)
        p.ell(24, 46, 36, 54, moss)                                 # moss patches
        p.ell(58, 80, 70, 88, moss)
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
