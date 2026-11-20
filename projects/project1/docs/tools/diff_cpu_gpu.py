"""Compare the CPU and GPU screenshots of the fixed-pose scene, pixel by pixel.

Usage:
    python docs/tools/diff_cpu_gpu.py [cpu.png] [gpu.png]

Defaults to docs/images/cpu_fixed_pose.png and docs/images/gpu_fixed_pose.png,
and rewrites docs/images/diff_3d_region_x24.png and diff_mask_gt32.png.
"""

from pathlib import Path
import sys

import numpy as np
from PIL import Image

DOCS = Path(__file__).resolve().parent.parent
IMAGES = DOCS / "images"

# Window body starts below the title bar. The 3D region excludes the ImGui panel
# on the left and the window border on the right/bottom.
TITLE_BAR = 40
REGION_ROWS = slice(20, 695)   # absolute y in [60, 735)
REGION_COLS = slice(420, 1270)


def compare(cpu_path: Path, gpu_path: Path) -> None:
    a = Image.open(cpu_path).convert("RGB")
    b = Image.open(gpu_path).convert("RGB")
    if a.size != b.size:
        raise SystemExit(f"size mismatch: {a.size} vs {b.size}")

    A = np.asarray(a, np.int16)[TITLE_BAR:, :]
    B = np.asarray(b, np.int16)[TITLE_BAR:, :]
    D = np.abs(A - B)
    whole = D.max(axis=2)

    print(f"image size                  : {a.size[0]}x{a.size[1]}")
    print(f"body: mean / max abs diff   : {D.mean():.4f} / {int(D.max())} of 255")
    print(f"body: identical pixels      : {(whole == 0).mean() * 100:.2f} %")
    print(f"body: pixels with diff > 8  : {(whole > 8).mean() * 100:.3f} %  (window border + UI state)")

    A3 = A[REGION_ROWS, REGION_COLS]
    B3 = B[REGION_ROWS, REGION_COLS]
    D3 = np.abs(A3 - B3).max(axis=2)
    changed = D3 > 0

    print("--- 3D region only (850x675) ---")
    print(f"identical pixels            : {(~changed).mean() * 100:.2f} %")
    print(f"max abs diff                : {int(D3.max())} / 255")
    print(f"median diff (changed px)    : {float(np.median(D3[changed])):.1f} / 255")

    gray = np.asarray(a.convert("L"), np.int16)[TITLE_BAR:, :][REGION_ROWS, REGION_COLS]
    grad = np.abs(np.diff(gray, axis=1, prepend=gray[:, :1])) + np.abs(
        np.diff(gray, axis=0, prepend=gray[:1, :])
    )
    print(f"edge strength at changed px : {grad[changed].mean():.1f} (all px {grad.mean():.1f})")

    Image.fromarray(np.clip(D3 * 24, 0, 255).astype(np.uint8)).save(IMAGES / "diff_3d_region_x24.png")
    Image.fromarray((whole > 32).astype(np.uint8) * 255).save(IMAGES / "diff_mask_gt32.png")


if __name__ == "__main__":
    cpu = Path(sys.argv[1]) if len(sys.argv) > 1 else IMAGES / "cpu_fixed_pose.png"
    gpu = Path(sys.argv[2]) if len(sys.argv) > 2 else IMAGES / "gpu_fixed_pose.png"
    compare(cpu, gpu)
