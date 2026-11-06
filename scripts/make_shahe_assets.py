"""Draw the Shahe rooms and reusable outdoor objects at their gameplay anchors."""
from pathlib import Path
import argparse
import random

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SCALE = 2
INK = "#304C4A"
PAPER = "#F8F3DF"
WOOD = "#E4C29C"
WOOD_DARK = "#9C6C50"
BLUE = "#9CBCC8"
GLASS = "#C6E2DF"
GREEN = "#AFC994"
BRICK = "#BF795F"


class Plate:
    def __init__(self, w, h, fill=(0, 0, 0, 0)):
        self.img = Image.new("RGBA", (w * SCALE, h * SCALE), fill)
        self.pen = ImageDraw.Draw(self.img)

    @staticmethod
    def coords(points):
        return tuple(round(p * SCALE) for p in points)

    def rect(self, bounds, fill, radius=0, outline=None):
        self.pen.rounded_rectangle(self.coords(bounds), radius * SCALE, fill, outline, SCALE)

    def oval(self, bounds, fill, outline=None):
        self.pen.ellipse(self.coords(bounds), fill, outline, SCALE)

    def poly(self, points, fill, outline=None):
        self.pen.polygon([self.coords(p) for p in points], fill, outline, SCALE)

    def line(self, points, fill, width=1):
        self.pen.line([self.coords(p) for p in points], fill, width * SCALE, joint="curve")

    def finish(self):
        w, h = self.img.size
        return self.img.resize((w // SCALE, h // SCALE), Image.Resampling.LANCZOS)


def door(p):
    p.rect((313, 94, 380, 238), "#CEAA85", 9)
    p.rect((320, 100, 372, 235), WOOD_DARK, 6)
    p.rect((324, 104, 368, 235), "#A4BDC0", 4)
    p.rect((331, 115, 361, 155), GLASS, 5, "#617D82")
    p.line([(343, 117), (343, 152)], PAPER)
    p.oval((355, 178, 361, 184), "#80624A")
    p.rect((319, 233, 373, 240), "#BA9375", 3)


def window(p, x, y, w):
    p.rect((x + 4, y + 6, x + w + 4, y + 81), "#CDCEBA", 6)
    p.rect((x, y, x + w, y + 75), PAPER, 5, "#90A6A2")
    p.rect((x + 6, y + 6, x + w - 6, y + 69), GLASS, 4)
    p.poly([(x + 7, y + 46), (x + 24, y + 40), (x + 41, y + 59),
            (x + 70, y + 31), (x + w - 7, y + 54), (x + w - 7, y + 68),
            (x + 7, y + 68)], GREEN)
    p.line([(x + w / 2, y + 5), (x + w / 2, y + 69)], PAPER, 4)
    p.line([(x + 6, y + 37), (x + w - 6, y + 37)], PAPER, 3)
    p.rect((x - 4, y + 70, x + w + 4, y + 77), WOOD, 3)


def shell(accent):
    p = Plate(390, 450, "#E6E9DB")
    p.rect((12, 18, 377, 224), "#EEF0E1", 19)
    p.rect((13, 214, 377, 433), "#EED5AE", 13)
    p.rect((15, 215, 376, 222), "#D3B68E", 2)
    for y in range(246, 430, 31):
        p.line([(19, y), (370, y)], "#DFC29D")
        for x in range(53 + (y % 5) * 18, 365, 88):
            p.line([(x, y), (x, min(430, y + 31))], "#E2C8A3")
    p.rect((21, 425, 370, 430), "#E8CAA0", 2)
    p.line([(24, 77), (292, 77)], accent)
    door(p)
    return p


def dorm():
    p = shell(BLUE)
    window(p, 188, 103, 101)
    p.poly([(216, 180), (275, 180), (308, 251), (254, 251)], "#F4EBD2")
    p.rect((173, 342, 310, 410), "#9DBABD", 31)
    p.rect((181, 350, 302, 402), "#ADC9C8", 25, PAPER)
    for y in (361, 392):
        p.line([(194, y), (286, y)], "#C9DBCE", 2)
    p.oval((38, 259, 176, 287), "#D0AD88")
    for x in (41, 143):
        p.rect((x, 121, x + 7, 277), WOOD_DARK, 3)
        p.line([(x + 2, 128), (x + 2, 272)], WOOD)
    p.rect((40, 221, 147, 237), WOOD, 4, WOOD_DARK)
    p.poly([(48, 213), (137, 213), (147, 221), (40, 221)], "#F5DEB3", WOOD_DARK)
    p.rect((66, 184, 116, 213), "#637D83", 4)
    p.rect((70, 188, 112, 208), "#B0D2C5", 2)
    p.line([(76, 195), (92, 195)], PAPER)
    p.line([(78, 201), (100, 201)], "#789D8E")
    p.rect((86, 213, 99, 219), "#637D83", 2)
    p.rect((56, 227, 79, 232), "#D5A577", 2)
    p.rect((100, 248, 134, 265), "#849FA5", 5, "#5A7B80")
    p.rect((97, 270, 138, 280), "#A9C3C3", 5, "#5A7B80")
    p.rect((105, 279, 109, 285), WOOD_DARK)
    p.rect((129, 279, 133, 285), WOOD_DARK)
    p.rect((36, 116, 152, 153), WOOD, 5, WOOD_DARK)
    p.rect((43, 119, 144, 142), "#B8D3CE", 7)
    p.rect((46, 120, 76, 135), "#F6F0D9", 5, "#E9DDC7")
    p.rect((76, 122, 143, 141), "#88B1B4", 5)
    p.line([(81, 128), (136, 128)], "#B6D4CC", 2)
    p.line([(53, 114), (53, 133)], WOOD_DARK, 3)
    p.line([(143, 113), (143, 137)], WOOD_DARK, 3)
    p.line([(52, 115), (144, 115)], WOOD_DARK, 3)
    for y in range(158, 282, 21):
        p.line([(153, y), (171, y + 2)], WOOD_DARK, 4)
        p.line([(153, y - 1), (171, y + 1)], WOOD, 2)
    p.line([(150, 136), (150, 283)], WOOD_DARK, 5)
    p.line([(169, 136), (169, 286)], WOOD_DARK, 5)
    p.line([(167, 140), (167, 283)], WOOD, 2)
    p.oval((206, 315, 270, 336), "#D0AD88")
    p.rect((212, 288, 258, 328), "#90AD9B", 5, "#5C8171")
    p.poly([(209, 288), (221, 279), (250, 279), (261, 288)], "#B9CDAA", "#5C8171")
    p.rect((229, 302, 241, 307), PAPER, 2)
    return p, (36, 145, 152, 153), (98, 263, 138, 280)


def classroom():
    p = shell(GREEN)
    window(p, 30, 101, 95)
    p.rect((153, 102, 285, 165), WOOD_DARK, 6)
    p.rect((158, 107, 280, 160), "#66887C", 3)
    for i, h in enumerate((9, 19, 15, 27)):
        p.rect((177 + 15 * i, 146 - h, 183 + 15 * i, 146), "#C9DEC2", 2)
    p.line([(171, 146), (239, 146)], PAPER)
    p.line([(252, 154), (269, 154)], PAPER, 2)
    p.rect((208, 184, 271, 219), WOOD, 4, WOOD_DARK)
    p.poly([(215, 176), (266, 176), (271, 184), (208, 184)], "#F0D6AC", WOOD_DARK)
    for x, y, active in ((58, 250, True), (208, 250, False), (58, 331, False)):
        p.oval((x - 3, y + 26, x + 77, y + 42), "#D3AF88")
        for dx in (5, 62):
            p.rect((x + dx, y + 4, x + dx + 5, y + 36), WOOD_DARK, 2)
        p.rect((x + 49, y + 24, x + 72, y + 38), "#9EB7B0", 4, "#607D76")
        p.poly([(x + 2, y - 7), (x + 63, y - 7), (x + 73, y + 10),
                (x, y + 10)], "#F1D5AD", WOOD_DARK)
        p.rect((x, y + 10, x + 73, y + 16), WOOD, 2, WOOD_DARK)
        if active:
            p.poly([(x + 16, y - 2), (x + 38, y - 3), (x + 45, y + 6),
                    (x + 20, y + 6)], PAPER, "#AA8F75")
            p.line([(x + 29, y - 2), (x + 33, y + 5)], "#AA8F75")
    return p, (58, 260, 131, 267), None


def canteen():
    p = shell(BRICK)
    p.rect((32, 98, 285, 207), "#D9B292", 6, WOOD_DARK)
    p.rect((40, 110, 277, 190), "#799B9C", 4)
    for x in (64, 138, 212):
        p.rect((x - 14, 142, x + 42, 186), "#9DB6AF", 4)
        p.oval((x, 162, x + 28, 171), PAPER)
        p.line([(x + 7, 153), (x + 12, 147), (x + 9, 141)], PAPER)
    for i in range(7):
        x = 35 + i * 35
        p.poly([(x, 99), (x + 35, 99), (x + 39, 124), (x, 124)],
               PAPER if i % 2 else BRICK)
    p.rect((33, 185, 284, 198), "#E2B48B", 4, WOOD_DARK)
    p.rect((82, 200, 250, 210), "#C69D7D", 3)
    for x, y, active in ((58, 253, True), (222, 253, False), (108, 353, False)):
        p.oval((x - 3, y + 39, x + 76, y + 56), "#D0AD88")
        p.rect((x + 49, y + 42, x + 72, y + 54), "#9DB7A5", 4, "#648574")
        p.rect((x + 9, y + 11, x + 15, y + 43), WOOD_DARK, 2)
        p.rect((x + 64, y + 11, x + 70, y + 43), WOOD_DARK, 2)
        p.rect((x, y, x + 77, y + 28), "#E4BF92", 10, WOOD_DARK)
        p.line([(x + 8, y + 23), (x + 68, y + 23)], "#F4DAB1")
        if active:
            p.oval((x + 23, y + 4, x + 56, y + 24), PAPER, "#AB937B")
            p.oval((x + 30, y + 8, x + 50, y + 19), "#A0AE76")
            p.line([(x + 64, y + 7), (x + 61, y + 21)], "#8C705B", 2)
    return p, (58, 276, 135, 283), None


def tree():
    p = Plate(82, 105)
    p.oval((12, 87, 79, 103), "#9AAD84")
    p.poly([(38, 95), (44, 45), (51, 44), (48, 93)], WOOD_DARK)
    for x, y, w, h, fill in ((5, 35, 57, 41, "#759A72"), (26, 19, 51, 51, "#86AA7D"),
                             (15, 8, 50, 52, "#9DBC88"), (6, 20, 42, 38, GREEN),
                             (25, 10, 33, 31, "#BCD2A3")):
        p.oval((x, y, x + w, y + h), fill)
    p.line([(31, 49), (40, 57), (45, 67)], "#81A37B", 2)
    p.oval((23, 26, 27, 30), "#F0E3AB")
    p.oval((51, 38, 54, 42), "#F0E3AB")
    return p.finish()


def building(kind):
    p = Plate(218, 170)
    p.oval((18, 143, 217, 168), "#A5B88B")
    side = ("#BDA48B", "#879B9D", "#B8957F")[kind]
    face = ("#EAD3B1", "#B9C8C3", "#E6C5A3")[kind]
    roof = ("#839EA9", "#7295A5", "#BD816A")[kind]
    p.poly([(177, 49), (209, 29), (209, 130), (177, 154)], side, INK)
    p.rect((13, 52, 178, 154), face, 4, "#778D86")
    p.poly([(6, 56), (33, 29), (205, 29), (179, 56)], roof, "#617F82")
    p.poly([(6, 56), (179, 56), (179, 62), (6, 62)], "#5D7D86")
    if kind == 1:
        p.poly([(48, 37), (65, 12), (177, 12), (154, 37)], "#97ADB4", "#617F82")
        p.poly([(71, 19), (84, 3), (155, 3), (142, 19)], BLUE, "#617F82")
    if kind == 2:
        p.rect((16, 98, 177, 108), BRICK, 2)
        for x in range(20, 170, 22):
            p.rect((x, 98, x + 9, 108), PAPER, 1)
    for y in (75, 116):
        for x in (27, 60, 112, 145):
            p.rect((x, y, x + 21, y + 22), "#72969F", 3)
            p.rect((x + 2, y + 2, x + 19, y + 19), GLASS, 2)
            p.line([(x + 10, y + 3), (x + 10, y + 19)], PAPER)
    p.rect((83, 115, 110, 154), "#587982", 4)
    p.rect((87, 120, 106, 151), "#ABC8C8", 3)
    p.line([(96, 121), (96, 152)], PAPER)
    return p.finish()


def rgba_pixels(image):
    data = image.tobytes()
    return zip(data[::4], data[1::4], data[2::4], data[3::4])


def grain(image, seed):
    rng = random.Random(seed)
    pixels = []
    for r, g, b, a in rgba_pixels(image):
        shift = rng.choice((-3, -2, -1, 0, 0, 1, 2, 3))
        pixels.append(tuple(max(0, min(255, ch + shift)) for ch in (r, g, b)) + (a,))
    image.putdata(pixels)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview", type=Path, required=True)
    args = parser.parse_args()
    args.preview.mkdir(parents=True, exist_ok=True)
    code = ['/* Generated by scripts/make_shahe_assets.py. */', '#include "pet_shahe_assets.h"']
    header = ['#ifndef PET_SHAHE_ASSETS_H', '#define PET_SHAHE_ASSETS_H', '#include "lvgl.h"']
    bounds = []

    def emit(name, im, alpha):
        im.save(args.preview / (name + ".png"))
        rgb, opacity = bytearray(), bytearray()
        for r, g, b, a in rgba_pixels(im):
            rgb.extend((((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)).to_bytes(2, "little"))
            if alpha:
                opacity.append(a)
        data = rgb + opacity
        code.append(f"static const LV_ATTRIBUTE_MEM_ALIGN uint8_t {name}_data[] = {{")
        code.extend(" " + ",".join(f"0x{b:02x}" for b in data[i:i + 24]) + ","
                    for i in range(0, len(data), 24))
        code.extend(("};", f"const lv_image_dsc_t {name} = {{",
                     ".header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565"
                     + ("A8" if alpha else "")
                     + f",.w={im.width},.h={im.height},.stride={im.width * 2}}},",
                     f".data_size=sizeof({name}_data),.data={name}_data}};"))
        header.append(f"extern const lv_image_dsc_t {name};")

    for i, draw in enumerate((dorm, classroom, canteen)):
        plate, front, extra = draw()
        background = grain(plate.finish(), i)
        emit(f"shahe_room_{i}", background, False)
        for tag, rectangle in (("front", front), ("seat", extra)):
            if rectangle:
                # Crop the thin foreground rails; the room's image is the source of truth.
                emit(f"shahe_{tag}_{i}", background.crop(rectangle), True)
                bounds.append((tag, i, rectangle[:2]))
        emit(f"shahe_building_{i}", building(i), True)
    emit("shahe_tree", tree(), True)
    for tag, i, (x, y) in bounds:
        name = f"shahe_{tag}_{i}_xy"
        code.append(f"const int16_t {name}[2] = {{{x}, {y}}};")
        header.append(f"extern const int16_t {name}[2];")
    header.append("#endif")
    for name, lines in (("pet_shahe_assets.c", code), ("pet_shahe_assets.h", header)):
        (ROOT / "src" / name).write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
