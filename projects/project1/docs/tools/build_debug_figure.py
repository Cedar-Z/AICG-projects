"""Assemble the staged-debugging figure (README 8.1) and print CPU/GPU agreement.

Inputs are the ten screenshots saved under docs/images as
    debug_<view>_<backend>.png     view in {shaded, flat, bary, uv, normal}
If a source file is missing it is simply skipped, so the script can be re-run
incrementally while the screenshots are being collected.
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

IMAGES = Path(__file__).resolve().parents[1] / "images"
VIEWS = [("shaded", "完整材质"), ("flat", "常量颜色"), ("bary", "重心坐标"), ("uv", "UV"), ("normal", "法线")]
TITLE_BAR = 40


def load_font(size, bold=False):
    names = ["msyhbd.ttc", "msyh.ttc"] if bold else ["msyh.ttc", "msyhbd.ttc"]
    for name in names + ["segoeuib.ttf", "segoeui.ttf"]:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def agreement(cpu, gpu):
    """Compare the two screenshots over the rendered area only."""
    a = np.asarray(cpu.convert("RGB")).astype(int)
    b = np.asarray(gpu.convert("RGB")).astype(int)
    diff = np.abs(a - b).max(axis=2)
    mask = np.zeros(diff.shape, bool)
    mask[TITLE_BAR + 8 : -6, 6:-6] = True
    # The ImGui panel is identical in both shots but its text is cleared before the
    # scene render, so leave it out to keep the numbers about the rasterizers.
    # The box is a bit larger than the panel because older captures show one row less.
    mask[TITLE_BAR + 50 : TITLE_BAR + 250, 55:400] = False
    values = diff[mask]
    return {
        "pixels": int(mask.sum()),
        "exact": float((values == 0).mean() * 100.0),
        "mean": float(values.mean()),
        "max": int(values.max()),
        "above2": int((values > 2).sum()),
    }


def build(tile_width=642):
    rows = []
    stats = []
    for view, label in VIEWS:
        cpu_path = IMAGES / f"debug_{view}_cpu.png"
        gpu_path = IMAGES / f"debug_{view}_gpu.png"
        if not cpu_path.exists() or not gpu_path.exists():
            print(f"skip {view}: screenshot missing")
            continue
        cpu = Image.open(cpu_path).convert("RGB")
        gpu = Image.open(gpu_path).convert("RGB")
        stats.append((view, label, agreement(cpu, gpu)))
        rows.append((label, cpu, gpu))

    if not rows:
        print("nothing to assemble")
        return

    height = int(round(tile_width * rows[0][1].height / rows[0][1].width))
    label_width = 150
    gap = 10
    sheet = Image.new(
        "RGB",
        (label_width + 2 * tile_width + gap, len(rows) * (height + gap) - gap),
        (255, 255, 255),
    )
    draw = ImageDraw.Draw(sheet)
    head = load_font(22, bold=True)
    body = load_font(20, bold=True)
    for index, (label, cpu, gpu) in enumerate(rows):
        top = index * (height + gap)
        sheet.paste(cpu.resize((tile_width, height), Image.LANCZOS), (label_width, top))
        sheet.paste(gpu.resize((tile_width, height), Image.LANCZOS), (label_width + tile_width + gap, top))
        draw.text((12, top + height // 2 - 14), label, fill=(30, 30, 30), font=body)
        if index == 0:
            draw.text((label_width + 8, top + height - 34), "CPU", fill=(255, 255, 255), font=head)
            draw.text((label_width + tile_width + gap + 8, top + height - 34), "GPU", fill=(255, 255, 255), font=head)
    # Black outlines so the tiles do not bleed into each other.
    for index in range(len(rows)):
        top = index * (height + gap)
        draw.rectangle((label_width, top, label_width + 2 * tile_width + gap - 1, top + height - 1),
                       outline=(200, 200, 200))
        draw.line((label_width + tile_width + gap // 2, top,
                   label_width + tile_width + gap // 2, top + height), fill=(200, 200, 200))

    out = IMAGES / "debug_stages.png"
    sheet.save(out)
    print(f"wrote {out}  ({sheet.width}x{sheet.height})")
    for view, label, s in stats:
        print(f"{view:7s} {label:6s} exact {s['exact']:6.3f}%  mean {s['mean']:.4f}  max {s['max']:3d}  >2 {s['above2']:4d}")

    print("\ntest pages:")
    for name in ("stage", "clip"):
        cpu_path = IMAGES / f"test_{name}_cpu.png"
        gpu_path = IMAGES / f"test_{name}_gpu.png"
        if not cpu_path.exists() or not gpu_path.exists():
            print(f"skip {name}: screenshot missing")
            continue
        s = agreement(Image.open(cpu_path), Image.open(gpu_path))
        print(f"{name:7s} exact {s['exact']:6.3f}%  mean {s['mean']:.4f}  max {s['max']:3d}  "
              f">2 {s['above2']:4d}  pixels {s['pixels']}")


if __name__ == "__main__":
    build(int(sys.argv[1]) if len(sys.argv) > 1 else 642)
