#!/usr/bin/env python3
"""Generate the action-button icons (32x32, ARGB8888) for the main screen.

Language-independent glyphs for the five actions: feed / play / wash / wc /
talk. Drawn with PIL on a transparent canvas at 4x and downsampled, then
emitted as LVGL ARGB8888 lv_image_dsc_t into components/ui/src/icons.c.

Run from the project root:  python3 tools/gen_icons.py
"""
import os
from PIL import Image, ImageDraw

SZ = 32
S = 4
ICONS = ["feed", "play", "wash", "wc", "talk", "coin", "gift", "shop"]


class P:
    def __init__(self, d):
        self.d = d

    def b(self, x0, y0, x1, y1):
        return [x0 * S, y0 * S, x1 * S, y1 * S]

    def ell(self, x0, y0, x1, y1, fill, outline=None, ow=0):
        self.d.ellipse(self.b(x0, y0, x1, y1), fill=fill,
                       outline=outline, width=int(ow * S) if outline else 0)

    def rr(self, x0, y0, x1, y1, r, fill, outline=None, ow=0):
        self.d.rounded_rectangle(self.b(x0, y0, x1, y1), radius=r * S, fill=fill,
                                 outline=outline, width=int(ow * S) if outline else 0)

    def poly(self, pts, fill, outline=None, ow=0):
        self.d.polygon([(x * S, y * S) for x, y in pts], fill=fill,
                       outline=outline, width=int(ow * S) if outline else 0)

    def line(self, x0, y0, x1, y1, fill, w=1):
        self.d.line([(x0 * S, y0 * S), (x1 * S, y1 * S)], fill=fill, width=max(1, int(w * S)))

    def arc(self, x0, y0, x1, y1, a0, a1, fill, w=1):
        self.d.arc(self.b(x0, y0, x1, y1), a0, a1, fill=fill, width=max(1, int(w * S)))


OL = (40, 30, 46, 255)   # soft dark outline


def draw(name):
    img = Image.new("RGBA", (SZ * S, SZ * S), (0, 0, 0, 0))
    p = P(ImageDraw.Draw(img))
    if name == "feed":  # burger
        bun = (232, 176, 96, 255)
        bun_dk = (206, 150, 74, 255)
        p.rr(5, 7, 27, 15, 7, bun, OL, 1.0)                 # top bun
        for sx, sy in ((11, 10), (16, 9), (21, 10)):        # sesame
            p.ell(sx - 1, sy - 1, sx + 1, sy + 1, (250, 240, 220, 255))
        p.rr(5, 14, 27, 18, 2, (120, 200, 110, 255))        # lettuce
        p.rr(6, 17, 26, 22, 2, (150, 92, 52, 255), OL, 0.8) # patty
        p.rr(5, 21, 27, 26, 4, bun_dk, OL, 1.0)             # bottom bun
    elif name == "play":  # soccer ball
        p.ell(5, 5, 27, 27, (250, 250, 250, 255), OL, 1.2)
        p.poly([(16, 12), (20, 15), (18, 20), (14, 20), (12, 15)], (40, 40, 46, 255))  # center pentagon
        for a in (90, 162, 234, 306, 18):
            import math
            x = 16 + 9 * math.cos(math.radians(a))
            y = 16 + 9 * math.sin(math.radians(a))
            p.ell(x - 1.4, y - 1.4, x + 1.4, y + 1.4, (40, 40, 46, 255))
    elif name == "wash":  # water droplet + splashes (shower)
        drop = (90, 180, 235, 255)
        p.poly([(16, 5), (24, 20), (16, 27), (8, 20)], drop, OL, 1.0)
        p.ell(13, 17, 19, 24, (150, 215, 245, 255))         # highlight
        p.ell(7, 9, 10, 12, drop)                            # side splashes
        p.ell(23, 8, 26, 11, drop)
    elif name == "wc":  # toilet (side view)
        white = (245, 245, 248, 255)
        p.rr(6, 5, 12, 16, 2, white, OL, 1.0)               # tank
        p.rr(6, 15, 26, 20, 3, white, OL, 1.0)              # seat/lid
        p.poly([(8, 20), (24, 20), (21, 28), (11, 28)], white, OL, 1.0)  # bowl
        p.ell(11, 21, 21, 25, (170, 205, 235, 255))         # water
    elif name == "coin":  # gold coin
        gold = (245, 205, 70, 255)
        gold_dk = (214, 168, 40, 255)
        p.ell(5, 5, 27, 27, gold, gold_dk, 1.2)
        p.ell(9, 9, 23, 23, (0, 0, 0, 0), (255, 232, 150, 255), 1.0)
        # a small 4-point sparkle in the middle (reads as "shiny coin")
        p.poly([(16, 10), (17.5, 15), (16, 22), (14.5, 15)], gold_dk)
        p.poly([(10, 16), (15, 14.5), (22, 16), (15, 17.5)], gold_dk)
    elif name == "gift":  # wrapped present
        box = (230, 120, 140, 255)
        box_dk = (200, 92, 112, 255)
        ribbon = (250, 232, 150, 255)
        p.rr(6, 13, 26, 27, 2, box, OL, 1.0)          # box
        p.rr(5, 10, 27, 15, 2, box_dk, OL, 1.0)       # lid
        p.rr(14, 10, 18, 27, 1, ribbon)               # vertical ribbon
        p.poly([(16, 10), (10, 4), (14, 10)], ribbon, OL, 0.6)   # bow left
        p.poly([(16, 10), (22, 4), (18, 10)], ribbon, OL, 0.6)   # bow right
    elif name == "shop":  # market stall with striped awning
        awning = (230, 120, 140, 255)
        stripe = (250, 240, 230, 255)
        wall = (240, 205, 150, 255)
        p.rr(7, 12, 25, 27, 2, wall, OL, 1.0)               # stall body
        p.rr(11, 17, 21, 27, 1, (150, 92, 52, 255), OL, 0.8)  # door/counter
        p.poly([(4, 12), (28, 12), (26, 4), (6, 4)], awning, OL, 1.0)  # awning
        for sx in (9, 15, 21):                              # awning stripes
            p.poly([(sx, 4.6), (sx + 3, 4.6), (sx + 3.6, 11.4), (sx + 0.6, 11.4)], stripe)
        p.ell(4, 10, 28, 14, awning, OL, 0.6)               # scalloped edge
    elif name == "talk":  # speech bubble
        bub = (215, 250, 233, 255)
        p.rr(5, 6, 27, 22, 6, bub, OL, 1.0)
        p.poly([(10, 21), (10, 28), (17, 22)], bub, OL, 1.0)  # tail
        for dx in (11, 16, 21):
            p.ell(dx - 1.4, 13, dx + 1.4, 15.8, (60, 120, 96, 255))
    return img.resize((SZ, SZ), Image.LANCZOS)


def argb8888(img):
    out = bytearray()
    for r, g, b, a in list(img.getdata()):
        out += bytes((b, g, r, a))  # LVGL ARGB8888 little-endian: B,G,R,A
    return bytes(out)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    assets = os.path.join(root, "assets")
    os.makedirs(assets, exist_ok=True)
    imgs = {n: draw(n) for n in ICONS}

    pad = 6
    grid = Image.new("RGBA", (pad + len(ICONS) * (SZ + pad), SZ + 2 * pad), (24, 18, 43, 255))
    for i, n in enumerate(ICONS):
        grid.alpha_composite(imgs[n], (pad + i * (SZ + pad), pad))
    grid.resize((grid.width * 4, grid.height * 4), Image.NEAREST).save(
        os.path.join(assets, "icons_preview.png"))

    c_path = os.path.join(root, "components/ui/src/icons.c")
    h_path = os.path.join(root, "components/ui/include/vaiwu/ui/icons.h")
    with open(c_path, "w") as c:
        c.write("/* Auto-generated by tools/gen_icons.py - do not edit by hand. */\n")
        c.write('#include "vaiwu/ui/icons.h"\n\n')
        for n in ICONS:
            data = argb8888(imgs[n])
            c.write(f"static const uint8_t icon_{n}_map[{len(data)}] = {{\n")
            for i in range(0, len(data), 24):
                c.write("    " + ",".join(str(x) for x in data[i:i + 24]) + ",\n")
            c.write("};\n\n")
            c.write(f"const lv_image_dsc_t vaiwu_icon_{n} = {{\n")
            c.write("    .header = {\n        .magic = LV_IMAGE_HEADER_MAGIC,\n")
            c.write("        .cf = LV_COLOR_FORMAT_ARGB8888,\n")
            c.write(f"        .w = {SZ},\n        .h = {SZ},\n        .stride = {SZ * 4},\n    }},\n")
            c.write(f"    .data_size = {len(data)},\n    .data = icon_{n}_map,\n}};\n\n")
    with open(h_path, "w") as h:
        h.write("/* Auto-generated by tools/gen_icons.py - do not edit by hand. */\n")
        h.write("#pragma once\n\n#include \"lvgl.h\"\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
        for n in ICONS:
            h.write(f"extern const lv_image_dsc_t vaiwu_icon_{n};\n")
        h.write("\n#ifdef __cplusplus\n}\n#endif\n")
    print("generated", len(ICONS), "icons")


if __name__ == "__main__":
    main()
