"""Genera la textura de parquet (albedo + rugosidad + normal) que usa M_Parquet.
Tablas de 12 cm a lo largo de la pista con juntas desfasadas, vetas y desgaste.
Uso: python Tools/make_parquet.py  (necesita pillow y numpy)"""
import numpy as np
from PIL import Image, ImageFilter
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "Textures")
N = 2048                      # 2K (subilo a 4096 si querés más detalle)
PLANKS = 24                   # tablas por tile (tile de 2,88 m)
rng = np.random.default_rng(7)

alb = np.zeros((N, N, 3), np.float32)
rough = np.full((N, N), 0.28, np.float32)
height = np.zeros((N, N), np.float32)
pw = N // PLANKS
base = np.array([0.62, 0.42, 0.22])
for i in range(PLANKS):
    tone = base * (0.86 + rng.random() * 0.28)
    shift = int(rng.random() * N)                          # juntas desfasadas
    plank = np.tile(tone, (N, pw, 1)).astype(np.float32)
    # vetas: ruido estirado a lo largo de la tabla
    g = rng.random((N // 64, pw // 4 + 2)).astype(np.float32)
    g = np.array(Image.fromarray((g * 255).astype(np.uint8)).resize((pw, N), Image.BICUBIC), np.float32) / 255.0
    plank *= (0.9 + 0.2 * g)[..., None]
    plank = np.roll(plank, shift, axis=0)
    alb[:, i * pw:(i + 1) * pw] = plank
    alb[:, i * pw:i * pw + 2] *= 0.35                       # junta longitudinal
    height[:, i * pw:i * pw + 3] = -1.0
    jz = (shift + np.arange(0, N, N // 5)) % N               # juntas transversales (tablas de 1,2 m)
    for z in jz:
        alb[z:z + 2, i * pw:(i + 1) * pw] *= 0.35
        height[z:z + 3, i * pw:(i + 1) * pw] = -1.0
# desgaste: rayas de ruedas, más rugoso donde se gasta el barniz
wear = np.array(Image.fromarray((rng.random((N // 8, N // 8)) * 255).astype(np.uint8)).resize((N, N), Image.BICUBIC), np.float32) / 255.0
rough += 0.18 * (wear > 0.72)
alb = np.clip(alb, 0, 1)
srgb = (np.power(alb, 1 / 2.2) * 255).astype(np.uint8)
Image.fromarray(srgb).save(os.path.join(OUT, "T_Parquet_BaseColor.png"))
Image.fromarray((np.clip(rough, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT, "T_Parquet_Roughness.png"))
h = np.array(Image.fromarray(((height + 1) * 127).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0
gx, gy = np.gradient(h)
nrm = np.dstack([-gx * 6, -gy * 6, np.ones_like(h)])
nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
Image.fromarray(((nrm * 0.5 + 0.5) * 255).astype(np.uint8)).save(os.path.join(OUT, "T_Parquet_Normal.png"))
print("ok:", os.path.abspath(OUT))
