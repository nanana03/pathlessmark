#!/usr/bin/env python
# -*- coding: utf-8 -*-
r"""オーバーレイバッジ20案の選択ツールを作る（2026-08-10）。

    python tools\make-badge-picker.py

入力：LocalAI\ComfyUI\output\fbadge_<motif>_s<seed>_*.png（黒ピクト・白地）
出力：assets\badges\<motif>_s<seed>.png（透過・単色化・1024）
      badge_picker.html（自己完結・file:// で動く）

■ 判定の要点
  オーバーレイは**フォルダアイコンの左下に、実寸 10〜16px** で描かれる。
  → プレビューは「**実際のフォルダアイコンの上に重ねた 16px**」を主役に置く。
     大きく見て良い形でも、16px で潰れたら不採用。
  → 白フチ付き（背景に埋もれないため）と、フチ無しの両方を出して比べる。
"""
import base64
import io
import re
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT.parent / "LocalAI" / "ComfyUI" / "output"
OUT = ROOT / "assets" / "badges"
OUT.mkdir(parents=True, exist_ok=True)

INK = (32, 96, 64)      # バッジの色（濃い緑）。決定後に調整可
TARGET = 0.88           # 極小で見せるので大きめに正規化


def to_alpha(path):
    """黒ピクト＋白地 → 単色＋透過（輝度をαに）。"""
    g = Image.open(path).convert("L")
    alpha = g.point(lambda v: 255 - v)
    solid = alpha.point(lambda v: 255 if v > 64 else 0)
    bbox = solid.getbbox()
    if not bbox:
        return None
    a = alpha.crop(bbox)
    side = 1024
    scale = TARGET * side / max(a.size)
    a = a.resize((max(1, int(a.width * scale)), max(1, int(a.height * scale))), Image.LANCZOS)
    canvas = Image.new("L", (side, side), 0)
    canvas.paste(a, ((side - a.width) // 2, (side - a.height) // 2))
    img = Image.new("RGBA", (side, side), INK + (0,))
    img.putalpha(canvas)
    return img


def with_outline(img, ratio=0.045):
    """白フチを付ける（フォルダの黄色や暗い壁紙に埋もれないように）。

    ⚠️ MaxFilter は窓サイズの2乗で効く。1024px に窓97で掛けたら**2分でも終わらなかった**
       （2026-08-10）。**小さく縮めてから膨張させ、拡大して戻す**のが正解。
    """
    side = img.width
    small = 256
    a = img.split()[3].resize((small, small), Image.LANCZOS)
    k = max(1, int(small * ratio)) * 2 + 1          # 256px 基準の窓（奇数）
    grown = a.filter(ImageFilter.MaxFilter(k)).resize((side, side), Image.LANCZOS)
    out = Image.new("RGBA", img.size, (255, 255, 255, 0))
    white = Image.new("RGBA", img.size, (255, 255, 255, 255))
    out.paste(white, (0, 0), grown)
    out.alpha_composite(img)
    return out


def uri(img, size):
    im = img.copy()
    im.thumbnail((size, size), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, "PNG")
    return "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode()


# 背景に敷く「フォルダらしい四角」（実際のフォルダアイコンの代役）
def folder_tile(px=64):
    im = Image.new("RGBA", (px, px), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    body = [px * 0.08, px * 0.28, px * 0.92, px * 0.86]
    tab = [px * 0.08, px * 0.18, px * 0.45, px * 0.30]
    d.rounded_rectangle(tab, radius=px * 0.05, fill=(228, 178, 74, 255))
    d.rounded_rectangle(body, radius=px * 0.07, fill=(245, 199, 92, 255))
    buf = io.BytesIO()
    im.save(buf, "PNG")
    return "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode()


cards = []
for f in sorted(SRC.glob("fbadge_*.png")):
    m = re.match(r"fbadge_([a-z]+)_s(\d+)", f.stem)
    if not m:
        continue
    motif, seed = m.group(1), m.group(2)
    img = to_alpha(f)
    if img is None:
        continue
    key = f"{motif}_s{seed}"
    img.save(OUT / f"{key}.png")
    outlined = with_outline(img)
    outlined.save(OUT / f"{key}_outlined.png")
    cards.append(f"""
<div class="card">
  <div class="name">{key}</div>
  <div class="row">
    <figure><div class="big" style="background-image:url({uri(img,256)})"></div>
      <figcaption>原寸（フチ無し）</figcaption></figure>
    <figure><div class="big" style="background-image:url({uri(outlined,256)})"></div>
      <figcaption>白フチ付き</figcaption></figure>
    <figure><div class="folder">
        <div class="badge16" style="background-image:url({uri(outlined,64)})"></div></div>
      <figcaption><b>実寸16px（重ね）</b></figcaption></figure>
    <figure><div class="folder">
        <div class="badge10" style="background-image:url({uri(outlined,64)})"></div></div>
      <figcaption>10px（最小）</figcaption></figure>
    <figure><div class="folderdark">
        <div class="badge16" style="background-image:url({uri(outlined,64)})"></div></div>
      <figcaption>暗い背景</figcaption></figure>
  </div>
</div>""")

TILE = folder_tile()
html = f"""<!DOCTYPE html><html lang="ja"><head><meta charset="utf-8">
<title>PathlessMark バッジ候補</title><style>
body{{font-family:"Yu Gothic UI",sans-serif;background:#eceff1;margin:20px}}
h1{{font-size:17px}}
.card{{background:#fff;border-radius:10px;padding:12px 16px;margin:10px 0;box-shadow:0 1px 3px rgba(0,0,0,.15)}}
.name{{font-size:13px;font-weight:bold;color:#555;margin-bottom:8px}}
.row{{display:flex;gap:26px;align-items:flex-end;flex-wrap:wrap}}
figure{{margin:0;text-align:center}} figcaption{{font-size:11px;color:#666;margin-top:5px}}
.big{{width:120px;height:120px;background-size:contain;background-repeat:no-repeat;background-position:center;
 border:1px solid #eee;background-color:#fafafa}}
.folder,.folderdark{{width:64px;height:64px;position:relative;background-image:url({TILE});
 background-size:contain;background-repeat:no-repeat}}
.folderdark{{background-color:#2b2b2b;padding:0}}
.badge16{{position:absolute;left:0;bottom:0;width:16px;height:16px;
 background-size:contain;background-repeat:no-repeat}}
.badge10{{position:absolute;left:0;bottom:0;width:10px;height:10px;
 background-size:contain;background-repeat:no-repeat}}
</style></head><body>
<h1>PathlessMark バッジ候補（{len(cards)}案）</h1>
<div style="font-size:12px;color:#666;margin-bottom:14px">
オーバーレイはフォルダアイコンの<b>左下に実寸 10〜16px</b> で描かれます。
<b>「実寸16px（重ね）」の列で判断してください</b>——大きく見て良い形でも、16pxで潰れたら不採用です。<br>
モチーフ： leaf=葉／dot=丸／dead=行き止まり標識／flag=旗／under=下向き矢印
</div>
{''.join(cards)}
</body></html>"""
io.open(ROOT / "badge_picker.html", "w", encoding="utf-8", newline="\n").write(html)
print(f"saved badge_picker.html  ({len(cards)}案)  透過PNG -> {OUT}")
