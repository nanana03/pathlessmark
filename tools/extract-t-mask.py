#!/usr/bin/env python
# -*- coding: utf-8 -*-
r"""採用図柄 dot_s55 の「t」の抜きを 1bit マスクにして src\badgeicon.h へ焼き込む。

    python tools\extract-t-mask.py            # 差分を表示するだけ
    python tools\extract-t-mask.py --write    # badgeicon.h の kTMask を置き換える

■ なぜマスクを焼き込むのか
  DLL は %APPDATA%\PathlessMark\badge.ico を**その場で描いて**作る（色を実行時に変えられる）。
  素材 PNG を実行時に読ませるとインストール先に素材を撒くことになるため、図柄は
  128x128 の 1bit マスク（2,048 バイト）としてヘッダに持つ。

■ 座標の決め事
  マスクは**円板の半径を ±1 に正規化**した正方形。描画側は色の円の半径でこの座標を引く
  （元図柄はフチが無く、円板いっぱいが色だったため）。どのサイズでも同じ形・同じ位置で入る。

■ 抽出のしかた
  dot_s55.png は「不透明＝円板／その中の透明＝t」。円板の中心と半径を α から求め、
  内側 0.86 までを見て α<0.5 の画素を t とする（円周のアンチエイリアスを拾わないため）。
  さらに近傍が2未満の孤立点を落とす。
"""
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "badges" / "dot_s55.png"
HDR = ROOT / "src" / "badgeicon.h"
N = 128


def build_mask():
    a = np.array(Image.open(SRC).convert("RGBA").split()[3]).astype(float) / 255.0
    ys, xs = np.nonzero(a > 0.5)
    cx, cy = xs.mean(), ys.mean()
    r = max(xs.max() - xs.min() + 1, ys.max() - ys.min() + 1) / 2

    out = np.zeros((N, N), dtype=np.uint8)
    for j in range(N):
        for i in range(N):
            u = (i + 0.5) / N * 2 - 1
            v = (j + 0.5) / N * 2 - 1
            if u * u + v * v > 0.86 ** 2:
                continue
            xi, yi = int(round(cx + u * r)), int(round(cy + v * r))
            if 0 <= xi < a.shape[1] and 0 <= yi < a.shape[0] and a[yi, xi] < 0.5:
                out[j, i] = 1

    # 孤立点を落とす（円周の取りこぼしがゴミとして残るのを防ぐ）
    p = np.pad(out, 1)
    nb = sum(p[1 + dy:1 + dy + N, 1 + dx:1 + dx + N]
             for dy in (-1, 0, 1) for dx in (-1, 0, 1)) - out
    out[(out == 1) & (nb < 3)] = 0
    return out


def to_c(mask):
    lines = []
    for j in range(N):
        row = []
        for k in range(N // 8):
            v = 0
            for bit in range(8):
                if mask[j, k * 8 + bit]:
                    v |= 1 << (7 - bit)
            row.append(v)
        lines.append("    " + ",".join(f"0x{x:02X}" for x in row) + ",")
    return "\n".join(lines) + "\n"


mask = build_mask()
body = to_c(mask)
ys, xs = np.nonzero(mask)
print(f"t の画素 {mask.sum()} / bbox x{xs.min()}-{xs.max()} y{ys.min()}-{ys.max()}")

text = HDR.read_text(encoding="utf-8")
pat = re.compile(r"(constexpr unsigned char kTMask\[kTN \* kTN / 8\] = \{\n)(.*?)(\};)", re.S)
m = pat.search(text)
if not m:
    sys.exit("badgeicon.h の kTMask が見つかりません")

if m.group(2) == body:
    print("差分なし（ヘッダは最新）")
elif "--write" in sys.argv:
    HDR.write_text(pat.sub(lambda x: x.group(1) + body + x.group(3), text, count=1),
                   encoding="utf-8", newline="\n")
    print("badgeicon.h を更新しました。build.ps1 → /writeico の再実行が要ります")
else:
    print("⚠️ ヘッダの内容と一致しません。--write で更新できます")
