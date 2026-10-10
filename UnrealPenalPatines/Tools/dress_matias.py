"""Viste al modelo Matias_175cm.glb (cuerpo desnudo de MakeHuman): pinta camiseta, short, medias, patines y guantes
sobre la textura de piel usando los pesos del esqueleto (sin tocar la malla), y extrae las demás texturas.
Salida en SourceAssets/Textures/Matias/. Uso: python Tools/dress_matias.py   (necesita pillow y numpy)"""
import json, struct, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
Image.MAX_IMAGE_PIXELS = None

HERE = os.path.dirname(os.path.abspath(__file__))
GLB = os.path.join(HERE, "..", "SourceAssets", "Models", "Matias_175cm.glb")
OUT = os.path.join(HERE, "..", "SourceAssets", "Textures", "Matias")
os.makedirs(OUT, exist_ok=True)

b = open(GLB, "rb").read()
off = 12; js = None; bo = None
while off < len(b):
    cl, ct = struct.unpack("<I4s", b[off:off + 8])
    if ct == b"JSON": js = json.loads(b[off + 8:off + 8 + cl])
    elif ct == b"BIN\0": bo = off + 8
    off += 8 + cl

def view(i):
    v = js["bufferViews"][i]; s = bo + v.get("byteOffset", 0)
    return b[s:s + v["byteLength"]], v.get("byteStride")

CT = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
NC = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
def acc(i):
    a = js["accessors"][i]; data, stride = view(a["bufferView"]); dt = np.dtype(CT[a["componentType"]]); n = NC[a["type"]]
    if stride and stride != dt.itemsize * n:
        arr = np.frombuffer(data, dtype=np.uint8)[a.get("byteOffset", 0):]
        rows = [np.frombuffer(arr[k * stride:k * stride + dt.itemsize * n].tobytes(), dtype=dt) for k in range(a["count"])]
        return np.array(rows)
    return np.frombuffer(data, dtype=dt, count=a["count"] * n, offset=a.get("byteOffset", 0)).reshape(a["count"], n) if n > 1 else \
        np.frombuffer(data, dtype=dt, count=a["count"], offset=a.get("byteOffset", 0))

# ---- extraer todas las imágenes y el mapa material -> textura ----
names = {}
for i, im in enumerate(js["images"]):
    data, _ = view(im["bufferView"])
    fn = os.path.join(OUT, "_raw_%d_%s.png" % (i, im.get("name")))
    open(fn, "wb").write(data); names[i] = fn
def mat_tex(m):
    pbr = m.get("pbrMetallicRoughness", {}); t = pbr.get("baseColorTexture")
    return js["textures"][t["index"]]["source"] if t else None
matmap = {m["name"]: mat_tex(m) for m in js["materials"]}
print("material -> imagen:", matmap)
for mname, label in (("Human.eyebrow001", "Brow"), ("Human.eyelashes01", "Lash"), ("Human.high-poly", "Eye"), ("Human.short01", "Hair"), ("Human.teeth_base", "Teeth"), ("Human.tongue01", "Tongue")):
    i = matmap.get(mname)
    if i is not None:
        im = Image.open(names[i]); im.thumbnail((1024, 1024)); im.save(os.path.join(OUT, "T_Matias_%s.png" % label)); print("ok", label)

# ---- malla del cuerpo: posiciones, UV, pesos (del SK_Matias.glb ya preparado: morphs horneados y sin la túnica) ----
GLB = os.path.join(HERE, "..", "SourceAssets", "Models", "SK_Matias.glb")
b = open(GLB, "rb").read()
off = 12
while off < len(b):
    cl, ct = struct.unpack("<I4s", b[off:off + 8])
    if ct == b"JSON": js = json.loads(b[off + 8:off + 8 + cl])
    elif ct == b"BIN\0": bo = off + 8
    off += 8 + cl
mesh = [m for m in js["meshes"] if m["name"] == "base"][0]; pr = mesh["primitives"][0]
P = acc(pr["attributes"]["POSITION"]).astype(np.float32)
UV = acc(pr["attributes"]["TEXCOORD_0"]).astype(np.float32)
NRM = acc(pr["attributes"]["NORMAL"]).astype(np.float32)
J = acc(pr["attributes"]["JOINTS_0"]).astype(np.int32)
Wt = acc(pr["attributes"]["WEIGHTS_0"]).astype(np.float32)
IDX = acc(pr["indices"]).astype(np.int64).reshape(-1, 3)
skin = js["skins"][0]; jn = [js["nodes"][j]["name"] for j in skin["joints"]]
print("vértices", len(P), "triángulos", len(IDX))

def klass(n):
    n = n.lower()
    if n.startswith(("wrist", "finger", "metacarpal")): return "glove"
    if n.startswith(("upperarm",)): return "sleeve"
    if n.startswith(("spine", "breast", "clavicle", "shoulder")): return "torso"
    if n.startswith(("pelvis", "root", "upperleg01")): return "shorts"
    if n.startswith(("lowerleg",)): return "sock"
    if n.startswith(("foot", "toe")): return "boot"
    return "skin"
CL = ["torso", "sleeve", "shorts", "sock", "boot", "glove"]
field = np.zeros((len(P), len(CL)), np.float32)
for k in range(4):
    for ci, c in enumerate(CL):
        m = np.array([klass(jn[j]) == c for j in J[:, k]])
        field[:, ci] += Wt[:, k] * m

# ---- rasterizar atributos (posición + campos) en el espacio UV a 2048 ----
R = 4096
attr = np.concatenate([P, field, NRM], axis=1)          # x,y,z + 6 campos + normal
buf = np.zeros((R, R, attr.shape[1]), np.float32); cov = np.zeros((R, R), bool)
uvp = UV * R
for tri in IDX:
    a, bb, c = uvp[tri[0]], uvp[tri[1]], uvp[tri[2]]
    x0, x1 = int(max(0, np.floor(min(a[0], bb[0], c[0])))), int(min(R - 1, np.ceil(max(a[0], bb[0], c[0]))))
    y0, y1 = int(max(0, np.floor(min(a[1], bb[1], c[1])))), int(min(R - 1, np.ceil(max(a[1], bb[1], c[1]))))
    if x1 < x0 or y1 < y0: continue
    den = (bb[1] - c[1]) * (a[0] - c[0]) + (c[0] - bb[0]) * (a[1] - c[1])
    if abs(den) < 1e-9: continue
    gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
    l1 = ((bb[1] - c[1]) * (gx - c[0]) + (c[0] - bb[0]) * (gy - c[1])) / den
    l2 = ((c[1] - a[1]) * (gx - c[0]) + (a[0] - c[0]) * (gy - c[1])) / den
    l3 = 1 - l1 - l2
    m = (l1 >= -.02) & (l2 >= -.02) & (l3 >= -.02)
    if not m.any(): continue
    val = l1[..., None] * attr[tri[0]] + l2[..., None] * attr[tri[1]] + l3[..., None] * attr[tri[2]]
    sub = buf[y0:y1 + 1, x0:x1 + 1]; sub[m] = val[m]; cov[y0:y1 + 1, x0:x1 + 1] |= m
X, Y, Z = buf[..., 0], buf[..., 1], buf[..., 2]
F = {c: buf[..., 3 + i] for i, c in enumerate(CL)}
NZ = buf[..., 3 + len(CL) + 2]

NAVY = np.array([31, 38, 87], np.float32); SKY = np.array([58, 154, 217], np.float32); WHITE = np.array([228, 232, 238], np.float32); BLACK = np.array([14, 15, 18], np.float32)
col = np.zeros((R, R, 3), np.float32); mask = np.zeros((R, R), np.float32)
def paint(sel, color):
    col[sel] = color; mask[sel] = 1
# camiseta: azul marino con bandas celeste y blanca, y puño blanco en la manga
jersey = ((F["torso"] > .5) & (Y > .90) & (Y < 1.5)) | ((F["sleeve"] > .5) & (np.abs(X) < .45))
paint(jersey, NAVY)
body = jersey & (F["torso"] > .5) & (np.abs(X) < .24)
# camiseta del club (como la foto): franja blanca, franja ancha celeste con el sponsor, franja blanca; cuello blanco
paint(body & (Y > 1.085) & (Y < 1.115), WHITE); paint(body & (Y >= 1.115) & (Y < 1.235), SKY); paint(body & (Y >= 1.235) & (Y < 1.265), WHITE)
paint(jersey & (F["torso"] > .5) & (Y > 1.462) & (Y < 1.5) & (np.abs(X) < .16), WHITE)
paint((F["sleeve"] > .5) & (np.abs(X) >= .36) & (np.abs(X) < .38), WHITE)
# short con vivo, medias con franja, patines y guantes
shorts = ((F["shorts"] > .4) | ((Y > .60) & (Y < .97))) & (Y > .56) & (Y < .97) & (np.abs(X) < .27) & ~jersey & (F["glove"] < .5) & (F["sock"] < .5) & cov
paint(shorts, NAVY); paint(shorts & (np.abs(X) > .14) & (np.abs(X) < .165), WHITE)
socks = (F["sock"] > .5) & (Y > .10)
paint(socks, NAVY); paint(socks & (Y > .33) & (Y < .37), WHITE)
paint((F["boot"] > .5) | ((F["sock"] > .5) & (Y <= .10)), BLACK)
paint((F["glove"] > .5) & (Y < 1.45), np.array([20, 21, 26], np.float32))
# bordes suaves
mask_img = Image.fromarray((mask * 255).astype(np.uint8)).resize((4096, 4096), Image.BICUBIC).filter(ImageFilter.GaussianBlur(1.2))
col_img = Image.fromarray(np.clip(col, 0, 255).astype(np.uint8)).resize((4096, 4096), Image.BICUBIC)
skin_img = Image.open(names[matmap["Human.body"]]).convert("RGB")
if skin_img.size != (4096, 4096): skin_img = skin_img.resize((4096, 4096))
# la tela conserva un poco del sombreado de la piel para que no se vea plana
lum = np.asarray(skin_img.convert("L"), np.float32) / 255.0
shade = (0.88 + 0.24 * (lum - lum.mean()))[..., None]
cloth = np.clip(np.asarray(col_img, np.float32) * shade, 0, 255)
m4 = (np.asarray(mask_img, np.float32) / 255.0)[..., None]
res = np.asarray(skin_img, np.float32) * (1 - m4) + cloth * m4
# ---- logos al frente (sponsor en la franja celeste, marca y escudo en el pecho), proyectados de frente sobre el torso ----
def up(a):
    return np.asarray(Image.fromarray(a.astype(np.float32), mode="F").resize((4096, 4096), Image.BILINEAR))
X4, Y4, NZ4 = up(X), up(Y), up(NZ)
front4 = up(((F["torso"] > .5) & jersey & cov).astype(np.float32)) > .5
front4 &= NZ4 > .35
PX = 4000.0                                                # píxeles por metro en el plano del frente
x0, x1, y0, y1 = -.26, .26, 1.0, 1.5
W_, H_ = int((x1 - x0) * PX), int((y1 - y0) * PX)
logo = Image.new("L", (W_, H_), 0); dr = ImageDraw.Draw(logo)
def font(sz, bold=True):
    for f in ("DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf", "LiberationSans-Bold.ttf", "arial.ttf"):
        for d in ("/usr/share/fonts/truetype/dejavu/", "/usr/share/fonts/truetype/liberation/", "C:/Windows/Fonts/", ""):
            try: return ImageFont.truetype(d + f, sz)
            except Exception: pass
    return ImageFont.load_default()
def at(x, y): return ((x - x0) * PX, (y1 - y) * PX)
def text_c(cx, cy, t, sz, bold=True, spacing=0):
    f = font(int(sz * PX)); bb = dr.textbbox((0, 0), t, font=f)
    w = bb[2] - bb[0] + spacing * (len(t) - 1) * PX; px, py = at(cx, cy)
    if spacing == 0: dr.text((px - w / 2 - bb[0], py - (bb[3] - bb[1]) / 2 - bb[1]), t, font=f, fill=255)
    else:
        x = px - w / 2
        for ch in t:
            cb = dr.textbbox((0, 0), ch, font=f); dr.text((x - cb[0], py - (bb[3] - bb[1]) / 2 - bb[1]), ch, font=f, fill=255); x += cb[2] - cb[0] + spacing * PX
text_c(0.0, 1.183, "AJEC", .062)
text_c(0.0, 1.133, "AUTOADHESIVOS", .02, bold=True, spacing=.002)
# marca (pecho derecho del jugador = izquierda de la foto) y escudo (pecho izquierdo)
cx, cy = -.09, 1.37
px, py = at(cx, cy + .02); r = .02 * PX
dr.rounded_rectangle((px - r, py - r, px + r, py + r), radius=r * .35, fill=255)
dr.rounded_rectangle((px - r * .55, py - r * .55, px + r * .55, py + r * .55), radius=r * .2, fill=0)
dr.ellipse((px - r * .22, py - r * .22, px + r * .22, py + r * .22), fill=255)
text_c(cx, cy - .022, "CALOU", .022)
sx, sy = .09, 1.37; s0 = .032 * PX; px, py = at(sx, sy)
shield = [(px - s0 * .8, py - s0), (px + s0 * .8, py - s0), (px + s0 * .8, py + s0 * .15), (px, py + s0), (px - s0 * .8, py + s0 * .15)]
dr.polygon(shield, fill=255)
dr.polygon([(px - s0 * .5, py - s0 * .7), (px + s0 * .5, py - s0 * .7), (px + s0 * .5, py + s0 * .1), (px, py + s0 * .65), (px - s0 * .5, py + s0 * .1)], fill=0)
dr.ellipse((px - s0 * .25, py - s0 * .45, px + s0 * .25, py + s0 * .05), fill=255)
la = np.asarray(logo, np.float32) / 255.0
u = np.clip((X4 - x0) * PX, 0, W_ - 1).astype(np.int32)      # visto de frente, +X del modelo (su izquierda) queda a la derecha de la imagen
v = np.clip((y1 - Y4) * PX, 0, H_ - 1).astype(np.int32)
k = la[v, u] * front4 * (X4 > x0) * (X4 < x1) * (Y4 > y0) * (Y4 < y1)
res = res * (1 - k[..., None]) + np.array([240, 243, 248], np.float32) * k[..., None]
Image.fromarray(np.clip(res, 0, 255).astype(np.uint8)).save(os.path.join(OUT, "T_Matias_BaseColor.png"))
prev = Image.open(os.path.join(OUT, "T_Matias_BaseColor.png")); prev.thumbnail((1000, 1000)); prev.save(os.path.join(HERE, "..", "SourceAssets", "Textures", "Matias", "_preview.png"))
print("ok T_Matias_BaseColor.png")
for f in os.listdir(OUT):
    if f.startswith("_raw_"): os.remove(os.path.join(OUT, f))
