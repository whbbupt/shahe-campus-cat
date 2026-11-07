"""Convert picture-book room plates, campus sprites and terrain to LVGL ROM assets."""
import argparse
import random
import re
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "assets/pet-shahe"
ROOM_SIZE = (390, 450)
CAMPUS_SIZE = (1120, 1040)
TILE_SIZE = (280, 260)
ROOM_NAMES = ("dorm", "classroom", "canteen")


def route():
    source = (ROOT / "src/pet_shahe_roads.inc").read_text(encoding="utf-8")
    nodes = {int(i): (int(x), int(y)) for i, x, y in re.findall(r"SHAHE_NODE\((\d+), (\d+), (\d+)\)", source)}
    return [(nodes[int(a)], nodes[int(b)]) for a, b in re.findall(r"SHAHE_ROAD\((\d+), (\d+)\)", source)]


def road_mask(edges, radius):
    mask = Image.new("L", CAMPUS_SIZE)
    pen = ImageDraw.Draw(mask)
    for (ax, ay), (bx, by) in edges:
        pen.line((ax, ay, bx, by), 255, radius * 2 + 1)
        for x, y in ((ax, ay), (bx, by)):
            pen.ellipse((x-radius, y-radius, x+radius, y+radius), 255)
    return mask


def campus_ground():
    rng = random.Random(8261)
    width, height = CAMPUS_SIZE
    low = Image.new("L", (22, 21))
    low.putdata([rng.randrange(256) for _ in range(22 * 21)])
    light = low.resize(CAMPUS_SIZE, Image.Resampling.BICUBIC).filter(ImageFilter.GaussianBlur(12))
    fine = (rng.randrange(-4, 5) for _ in range(width * height))
    grass = Image.new("RGB", CAMPUS_SIZE)
    grass.putdata([(155 + l//12+n, 174 + l//14+n, 116 + l//13+n)
                   for l, n in zip(light.tobytes(), fine)])
    details = Image.new("RGBA", CAMPUS_SIZE)
    pen = ImageDraw.Draw(details)
    for _ in range(1850):
        x, y = rng.randrange(width), rng.randrange(height)
        spread = rng.randrange(2, 7)
        tint = rng.choice(((106, 138, 78, 88), (219, 225, 153, 96), (240, 220, 151, 114)))
        pen.line((x, y, x-2, y-spread), tint)
        if rng.randrange(4) == 0:
            pen.line((x, y, x+3, y-3), tint)
    for x, y in ((87,174),(395,210),(708,192),(926,326),(352,554),(705,697),(857,790),(478,877)):
        for _ in range(20):
            px, py = x+rng.randrange(-23,24), y+rng.randrange(-12,13)
            pen.ellipse((px,py,px+3,py+3), rng.choice(((250,231,167,205),(230,238,194,190))))
    # A small planted pond marks the south section of Guomai Road.
    bank = [(321,686),(324,674),(340,663),(368,660),(384,650),(423,650),
            (447,661),(475,657),(504,662),(525,676),(527,688),(510,702),
            (483,708),(449,703),(422,711),(384,711),(362,706),(337,704)]
    shore = Image.new("L", CAMPUS_SIZE)
    ImageDraw.Draw(shore).polygon(bank, 255)
    shore = shore.filter(ImageFilter.GaussianBlur(4)).point(lambda a: min(255, max(0, (a-100)*4)))
    details.paste((122, 153, 113, 255), mask=shore)
    water = shore.filter(ImageFilter.MinFilter(7))
    water_pen = Image.new("RGBA", CAMPUS_SIZE)
    wave = ImageDraw.Draw(water_pen)
    for y in range(648,715):
        depth = (y-648)/67
        wave.line((316,y,531,y), (round(151+19*depth), round(184+22*depth), round(174+15*depth), 255))
    for x, y, length in ((353,673,43),(432,679,51),(394,689,66),(351,696,37),(473,696,25)):
        wave.arc((x,y,x+length,y+4), 6, 174, (225, 234, 206, 255), 1)
    details.paste(water_pen, mask=water)
    for x, y in ((332,669),(367,658),(425,653),(510,670),(504,700),(399,709)):
        for _ in range(13):
            px, py = x+rng.randrange(-6,7), y+rng.randrange(-3,4)
            pen.line((px,py,px+rng.randrange(-2,3),py-rng.randrange(2,7)),
                     rng.choice(((111,146,91,225),(144,168,93,230),(188,189,112,230))))
    grass = Image.alpha_composite(grass.convert("RGBA"), details).convert("RGB")

    edges = route()
    shadow = ImageChops.offset(road_mask(edges, 38), 3, 6).filter(ImageFilter.GaussianBlur(4))
    grass.paste((103, 115, 78), mask=shadow.point(lambda x: x*3//10))
    grass.paste((155, 153, 116), mask=road_mask(edges, 36).filter(ImageFilter.GaussianBlur(.5)))
    grass.paste((247, 226, 185), mask=road_mask(edges, 34))
    paving = Image.new("RGB", CAMPUS_SIZE)
    paving.putdata([(226+l//17+n, 203+l//19+n, 163+l//21+n)
                    for l, n in zip(light.tobytes(), (rng.randrange(-3,4) for _ in range(width*height)))])
    pen = ImageDraw.Draw(paving)
    for y in range(4,height,18):
        pen.line((0,y,width,y), (197,175,139))
        pen.line((0,y+1,width,y+1), (239,218,180))
        for x in range((y//18%2)*18,width,36):
            pen.line((x,y+1,x,y+17), (204,184,147))
    mask = road_mask(edges, 32)
    grass.paste(paving, mask=mask)
    for (ax, ay), (bx, by) in edges:
        # Contract points must stay on the same road throughout each segment.
        for k in range(21):
            point = (round(ax+(bx-ax)*k/20), round(ay+(by-ay)*k/20))
            assert mask.getpixel(point) == 255, point
    return grass


def furniture_layer(room, polygon):
    mask = Image.new("L", ROOM_SIZE)
    ImageDraw.Draw(mask).polygon(polygon, 255)
    bounds = mask.getbbox()
    layer = room.convert("RGBA")
    layer.putalpha(mask)
    return layer.crop(bounds), bounds[:2]


def sprites():
    atlas = Image.open(ART / "campus-atlas.png").convert("RGBA")
    w, h = atlas.size
    assert atlas.getchannel("A").getextrema() == (0, 255)
    for i, size in enumerate(((218, 182), (254, 187), (236, 160), (82, 106))):
        x, y = i % 2, i // 2
        sprite = atlas.crop((x*w//2, y*h//2, (x+1)*w//2, (y+1)*h//2))
        bbox = sprite.getchannel("A").point(lambda a: 255 if a >= 16 else 0).getbbox()
        assert bbox and bbox[0] > 0 and bbox[1] > 0 and bbox[2] < sprite.width and bbox[3] < sprite.height, (i, bbox)
        yield ImageOps.contain(sprite.crop(bbox), size, method=Image.Resampling.LANCZOS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview", type=Path, required=True)
    args = parser.parse_args()
    args.preview.mkdir(parents=True, exist_ok=True)
    code = ['/* Generated by scripts/make_shahe_assets.py. */', '#include "pet_shahe_assets.h"']
    header = ['#ifndef PET_SHAHE_ASSETS_H', '#define PET_SHAHE_ASSETS_H', '#include "lvgl.h"']
    payload = 0

    def emit(name, im, alpha=False):
        nonlocal payload
        im = im.convert("RGBA")
        im.save(args.preview / (name + ".png"))
        rgb = bytearray()
        channels = im.tobytes()
        for r, g, b in zip(channels[::4], channels[1::4], channels[2::4]):
            rgb.extend((((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)).to_bytes(2, "little"))
        data = rgb + (channels[3::4] if alpha else b"")
        payload += len(data)
        code.append(f"static const LV_ATTRIBUTE_MEM_ALIGN uint8_t {name}_data[] = {{")
        code.extend(" " + ",".join(f"0x{b:02x}" for b in data[i:i+24]) + "," for i in range(0, len(data), 24))
        code.extend(("};", f"const lv_image_dsc_t {name} = {{",
                     ".header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565"
                     + ("A8" if alpha else "")
                     + f",.w={im.width},.h={im.height},.stride={im.width*2}}},",
                     f".data_size=sizeof({name}_data),.data={name}_data}};"))
        header.append(f"extern const lv_image_dsc_t {name};")

    def foreground(name, room, polygon):
        layer, (x,y) = furniture_layer(room, polygon)
        emit(name, layer, True)
        code.append(f"const int16_t {name}_xy[2] = {{{x}, {y}}};")
        header.append(f"extern const int16_t {name}_xy[2];")

    masks = (
        ((25,155),(150,155),(153,165),(25,166)),
        ((67,268),(106,265),(110,271),(109,280),(69,281)),
        ((63,266),(108,265),(112,272),(109,282),(65,282)),
    )
    for i, room_name in enumerate(ROOM_NAMES):
        room = Image.open(ART / (room_name + ".png")).convert("RGB")
        room = ImageOps.fit(room, ROOM_SIZE, method=Image.Resampling.LANCZOS)
        emit(f"shahe_room_{i}", room)
        foreground(f"shahe_front_{i}", room, masks[i])
        if i == 0:
            foreground("shahe_seat_0", room, ((80,232),(119,230),(121,244),(82,248)))
    for name, sprite in zip(("shahe_building_0", "shahe_building_1", "shahe_building_2", "shahe_tree"), sprites()):
        emit(name, sprite, True)
    shadow = Image.new("RGBA", (66, 24))
    shadow.putdata([
        (55, 73, 44, round(82 * max(0, 1 - ((x-32)/32)**2 - ((y-12)/11)**2)**2))
        for y in range(24) for x in range(66)
    ])
    emit("shahe_tree_shadow", shadow, True)
    terrain = campus_ground()
    terrain.save(args.preview / "campus-terrain.png")
    tiles = []
    for y in range(0,CAMPUS_SIZE[1],TILE_SIZE[1]):
        for x in range(0,CAMPUS_SIZE[0],TILE_SIZE[0]):
            name = f"shahe_ground_{len(tiles)}"
            emit(name, terrain.crop((x,y,x+TILE_SIZE[0],y+TILE_SIZE[1])))
            tiles.append(name)
    code.append("const lv_image_dsc_t *const shahe_ground[16] = {"
                + ", ".join("&"+name for name in tiles) + "};")
    header.append("extern const lv_image_dsc_t *const shahe_ground[16];")
    header.append("#endif")
    for name, lines in (("pet_shahe_assets.c", code), ("pet_shahe_assets.h", header)):
        (ROOT / "src" / name).write_text("\n".join(lines)+"\n", encoding="utf-8")
    print(f"Compiled {payload:,} image bytes; terrain {CAMPUS_SIZE[0]}×{CAMPUS_SIZE[1]} split into {len(tiles)} tiles.")


if __name__ == "__main__":
    main()
