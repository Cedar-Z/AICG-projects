"""Annotate the debug test-page screenshots used by README section 8.2.

The test page is laid out on a fixed 5 x 2 grid for the "stage" page and a fixed
marker row for the "clip" page, so the cells can be located by constant pixel
rectangles.  Running this script on a raw screenshot produces the figure that
goes into the report; the geometry is page specific, not screenshot specific,
which is why the same rectangles work for the CPU and the GPU pass.

Usage:
    python annotate_test_page.py <raw.png> <stage|clip> <out.png>
"""

import sys

from PIL import Image, ImageDraw, ImageFont

# Cell rectangles measured on the 1284x770 window screenshots (including the
# 40 px title bar).  Each entry is (number, x0, y0, x1, y1, label).
STAGE_CELLS = [
    (1, 30, 305, 240, 485, ""),
    (2, 278, 300, 500, 500, "empty"),
    (3, 532, 300, 754, 500, "empty"),
    (4, 786, 300, 992, 500, "empty"),
    (5, 1000, 300, 1272, 495, ""),
    (6, 22, 550, 275, 752, ""),
    (7, 285, 545, 495, 720, ""),
    (8, 550, 540, 748, 738, ""),
    (9, 790, 540, 1000, 730, ""),
    (10, 1012, 590, 1280, 680, ""),
]

# The clipped triangle itself fills the frame, so only the three depth markers
# get a frame; the triangle is described in the figure caption.
CLIP_CELLS = [
    (1, 125, 635, 295, 748, "z = 1.20"),
    (2, 570, 610, 715, 710, "z = 0.85"),
    (3, 886, 586, 1045, 700, "z = 0.50"),
]

PAGES = {"stage": STAGE_CELLS, "clip": CLIP_CELLS}

GREEN = (70, 220, 120)
GREY = (150, 150, 150)


def load_font(size):
    for name in ("msyhbd.ttc", "msyh.ttc", "segoeuib.ttf", "segoeui.ttf", "arialbd.ttf", "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def dashed_box(draw, rect):
    x0, y0, x1, y1 = rect
    for x in range(x0, x1, 16):
        draw.line([(x, y0), (min(x + 8, x1), y0)], fill=GREY, width=2)
        draw.line([(x, y1), (min(x + 8, x1), y1)], fill=GREY, width=2)
    for y in range(y0, y1, 16):
        draw.line([(x0, y), (x0, min(y + 8, y1))], fill=GREY, width=2)
        draw.line([(x1, y), (x1, min(y + 8, y1))], fill=GREY, width=2)


def draw_label(draw, xy, text, font, color):
    x, y = xy
    b = draw.textbbox((0, 0), text, font=font)
    draw.rectangle((x - 4, y - 3, x + b[2] + 4, y + b[3] + 3), fill=(18, 18, 18))
    draw.text((x, y), text, fill=color, font=font)


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 1

    src, page, dst = sys.argv[1], sys.argv[2], sys.argv[3]
    cells = PAGES[page]
    font = load_font(26)
    small = load_font(19)

    image = Image.open(src).convert("RGB")
    draw = ImageDraw.Draw(image)

    for number, x0, y0, x1, y1, label in cells:
        if label == "empty":
            dashed_box(draw, (x0, y0, x1, y1))
            draw_label(draw, (x0 + 14, (y0 + y1) // 2 - 12), "空", small, GREY)
        else:
            draw.rectangle((x0, y0, x1, y1), outline=GREEN, width=2)

        text = str(number)
        b = draw.textbbox((0, 0), text, font=font)
        th = b[3] - b[1]
        cx, cy = x0, y0 - th - 12
        if cy < 46:
            cx, cy = x0 + 5, y0 + 5
        draw_label(draw, (cx, cy), text, font, (255, 255, 255))

        if label and label != "empty":
            ly = y0 - th - 16
            if ly < 46:
                ly = y1 + 6
            draw_label(draw, (x0 + 32, ly), label, small, GREEN)

    image.save(dst)
    print("wrote", dst)
    return 0


if __name__ == "__main__":
    sys.exit(main())
