"""Genera la textura de publicidad de la valla (T_Publicidad.png). Uso: python Tools/make_ads.py"""
from PIL import Image, ImageDraw, ImageFont
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "Textures", "T_Publicidad.png")
W, H = 2048, 96
img = Image.new("RGB", (W, H), (240, 242, 246))
d = ImageDraw.Draw(img)
def font_for(text, max_w, size=70):
    """Achica la letra hasta que el texto entre en su recuadro."""
    while size > 12:
        try:
            f = ImageFont.load_default(size=size)
        except TypeError:
            return ImageFont.load_default()
        bb = d.textbbox((0, 0), text, font=f)
        if bb[2] - bb[0] <= max_w:
            return f
        size -= 2
    return f
texts = ["HOCKEY SOBRE PATINES", "TORNEO APERTURA", "BOCHA Y PALO", "BUENOS AIRES", "TORNEO APERTURA", "HOCKEY SOBRE PATINES"]
cols = [(22, 35, 61), (200, 16, 46), (22, 35, 61), (42, 94, 168), (200, 16, 46), (22, 35, 61)]
seg = W // len(texts)
for i, (t, c) in enumerate(zip(texts, cols)):
    x0 = i * seg
    d.rectangle([x0, 0, x0 + seg - 4, H], fill=(255, 255, 255) if i % 2 == 0 else (225, 230, 240))
    font = font_for(t, seg - 60)
    bb = d.textbbox((0, 0), t, font=font)
    d.text((x0 + (seg - (bb[2] - bb[0])) / 2, (H - (bb[3] - bb[1])) / 2 - bb[1]), t, fill=c, font=font)
img.save(OUT)
print("ok", OUT)
