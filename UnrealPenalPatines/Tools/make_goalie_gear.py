"""Genera el equipo del arquero como modelos .glb (metros, +Y arriba, +Z adelante):
  SM_Polaina.glb  polaina de hockey sobre patines (origen abajo, centro de la cara de atrás; alto 1 m: se escala en el juego)
  SM_Guante.glb   guante de atrapar (origen en el centro de la palma; +Y hacia los dedos, +Z hacia donde mira la palma)
  SM_Blocker.glb  guante del palo con tabla (mismo marco que el guante; la tabla va del lado del dorso, -Z)
  SM_Careta.glb   careta (de SM_CarataArquero.gltf, "Hockey Goalie Mask" de matýsek, CC BY 4.0): mira a +Z, origen en el centro de la cabeza
Uso: python Tools/make_goalie_gear.py   (necesita numpy)"""
import json, struct, os, base64
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.join(HERE, "..", "SourceAssets", "Models")

def write_glb(path, prims, extras=None):
    """prims: lista de (nombre_material, color_rgba, P[n,3], N[n,3], I[m])"""
    buf = bytearray(); views = []; accs = []; meshes_prims = []; mats = []
    def add(arr, target, comp, typ, mm=False):
        while len(buf) % 4: buf.append(0)
        off = len(buf); b = arr.tobytes(); buf.extend(b)
        views.append({"buffer": 0, "byteOffset": off, "byteLength": len(b), "target": target})
        a = {"bufferView": len(views) - 1, "componentType": comp, "count": int(arr.shape[0]), "type": typ}
        if mm: a["min"] = arr.min(0).tolist(); a["max"] = arr.max(0).tolist()
        accs.append(a); return len(accs) - 1
    for name, col, P, N, I in prims:
        P = P.astype(np.float32); N = (N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)).astype(np.float32)
        ip = add(P, 34962, 5126, "VEC3", True); inn = add(N, 34962, 5126, "VEC3"); ii = add(I.astype(np.uint32), 34963, 5125, "SCALAR")
        mats.append({"name": name, "pbrMetallicRoughness": {"baseColorFactor": list(col), "metallicFactor": 0.0, "roughnessFactor": 0.6}})
        meshes_prims.append({"attributes": {"POSITION": ip, "NORMAL": inn}, "indices": ii, "material": len(mats) - 1})
    g = {"asset": {"version": "2.0", "generator": "make_goalie_gear.py", **({"extras": extras} if extras else {})},
         "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": os.path.basename(path)[:-4]}],
         "meshes": [{"name": os.path.basename(path)[:-4], "primitives": meshes_prims}], "materials": mats,
         "buffers": [{"byteLength": len(buf)}], "bufferViews": views, "accessors": accs}
    j = json.dumps(g, separators=(",", ":")).encode()
    while len(j) % 4: j += b" "
    while len(buf) % 4: buf.append(0)
    out = struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(j) + 8 + len(buf)) + struct.pack("<I4s", len(j), b"JSON") + j + struct.pack("<I4s", len(buf), b"BIN\0") + bytes(buf)
    open(path, "wb").write(out); print("ok", path, len(out), "bytes")

def sweep(profile_fn, ys, closed_caps=True):
    """Barre un perfil cerrado (función y -> puntos [k,2] en x,z) a lo largo de Y."""
    rings = [profile_fn(y) for y in ys]; k = len(rings[0])
    P = np.concatenate([np.column_stack([r[:, 0], np.full(k, y), r[:, 1]]) for r, y in zip(rings, ys)])
    I = []
    for a in range(len(ys) - 1):
        for i in range(k):
            j = (i + 1) % k; p0, p1, p2, p3 = a * k + i, a * k + j, (a + 1) * k + j, (a + 1) * k + i
            I += [p0, p2, p1, p0, p3, p2]
    I = np.array(I, np.int64)
    if closed_caps:
        for a, flip in ((0, True), (len(ys) - 1, False)):
            c = len(P); P = np.vstack([P, [[rings[a][:, 0].mean(), ys[a], rings[a][:, 1].mean()]]])
            tri = []
            for i in range(k):
                j = (i + 1) % k
                tri += ([c, a * k + i, a * k + j] if flip else [c, a * k + j, a * k + i])
            I = np.concatenate([I, np.array(tri)])
    return P, I

def normals(P, I):
    N = np.zeros_like(P); T = I.reshape(-1, 3)
    fn = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    for c in range(3): np.add.at(N, T[:, c], fn)
    return N

def orient(P, I):
    """Da vuelta los triángulos si las normales apuntan hacia adentro (según el centro)."""
    T = I.reshape(-1, 3); c = P.mean(0)
    fn = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]]); cen = P[T].mean(1)
    if (np.einsum("ij,ij->i", fn, cen - c) < 0).mean() > .5: T = T[:, ::-1]
    return T.reshape(-1)

# ---------------- polaina: perfil con 5 tubos verticales al frente, rodillera y puntera ----------------
def pad_profile(y):
    W = .27; D0 = .075                      # ancho y espesor base
    knee = np.exp(-((y - .62) / .08) ** 2) * .025
    toe = np.exp(-((y - .05) / .06) ** 2) * .02
    n = 80; pts = []
    for i in range(n):
        t = i / n * 2 * np.pi
        x = W / 2 * np.sign(np.cos(t)) * abs(np.cos(t)) ** .35
        z = np.sin(t)
        if z >= 0:   # frente con tubos
            u = (x + W / 2) / W
            rib = .012 * abs(np.cos(u * 5 * np.pi)) ** .6
            zz = (D0 + knee + toe + rib) * abs(z) ** .3
        else:
            zz = -.012 * abs(z) ** .5   # la parte de atrás casi plana (va contra la pierna)
        pts.append((x, zz))
    pts = np.array(pts)
    taper = 1.0 - .08 * max(0, y - .8) / .2
    pts[:, 0] *= taper
    return pts
ys = np.concatenate([np.linspace(0, .02, 3), np.linspace(.04, .96, 40), np.linspace(.98, 1.0, 3)])
P, I = sweep(pad_profile, ys); I = orient(P, I)
write_glb(os.path.join(MOD, "SM_Polaina.glb"), [("Polaina", (.95, .95, .95, 1), P, normals(P, I.reshape(-1)), I)])

# ---------------- guante de atrapar (tipo trapper): disco grueso curvado + puño ----------------
def ellipsoid(c, r, nu=32, nv=20, bend=0.0):
    u = np.linspace(0, 2 * np.pi, nu, endpoint=False); v = np.linspace(-np.pi / 2, np.pi / 2, nv)
    P = []; 
    for vv in v:
        for uu in u:
            x = r[0] * np.cos(vv) * np.cos(uu); y = r[1] * np.cos(vv) * np.sin(uu); z = r[2] * np.sin(vv)
            z += bend * (x * x + y * y)
            P.append((c[0] + x, c[1] + y, c[2] + z))
    P = np.array(P); I = []
    for a in range(nv - 1):
        for i in range(nu):
            j = (i + 1) % nu; p0, p1, p2, p3 = a * nu + i, a * nu + j, (a + 1) * nu + j, (a + 1) * nu + i
            I += [p0, p1, p2, p0, p2, p3]
    I = np.array(I); return P, orient(P, I)
def merge(parts):
    P = []; I = []; o = 0
    for p, i in parts: P.append(p); I.append(i + o); o += len(p)
    return np.vstack(P), np.concatenate(I)
glove_body = ellipsoid((0, .05, .0), (.13, .16, .04), bend=-.9)       # bolsillo cóncavo hacia +Z
glove_cuff = ellipsoid((0, -.1, -.01), (.07, .06, .055))
gP, gI = merge([glove_body, glove_cuff])
write_glb(os.path.join(MOD, "SM_Guante.glb"), [("Guante", (.95, .95, .95, 1), gP, normals(gP, gI), gI)])

# ---------------- blocker: tabla rectangular en el dorso + guante ----------------
def box(c, s, r=.012, n=6):
    P, I = ellipsoid(c, (s[0] / 2, s[1] / 2, s[2] / 2), nu=40, nv=16)
    # empuja la elipse hacia un rectángulo redondeado
    d = (P - np.array(c)) / (np.array(s) / 2)
    k = np.max(np.abs(d), axis=1, keepdims=True) ** .85
    P = np.array(c) + d / np.maximum(k, 1e-6) * (np.array(s) / 2) * (np.abs(d) ** .15)
    return P, I
board = box((0, .03, -.05), (.24, .36, .035))
hand = ellipsoid((0, .0, .0), (.055, .08, .035))
cuff = ellipsoid((0, -.1, -.01), (.06, .05, .05))
bP, bI = merge([board, hand, cuff])
write_glb(os.path.join(MOD, "SM_Blocker.glb"), [("Blocker", (.95, .95, .95, 1), bP, normals(bP, bI), bI)])

# ---------------- careta: del modelo de Sketchfab, normalizada ----------------
src = os.path.join(MOD, "SM_CarataArquero.gltf")
js = json.load(open(src))
raw = base64.b64decode(js["buffers"][0]["uri"].split(",", 1)[1])
def acc(i):
    a = js["accessors"][i]; bv = js["bufferViews"][a["bufferView"]]
    n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
    dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a["componentType"]]
    off = bv.get("byteOffset", 0) + a.get("byteOffset", 0); st = bv.get("byteStride")
    if st and st != np.dtype(dt).itemsize * n:
        return np.array([np.frombuffer(raw[off + k * st: off + k * st + np.dtype(dt).itemsize * n], dtype=dt) for k in range(a["count"])])
    arr = np.frombuffer(raw[off: off + a["count"] * n * np.dtype(dt).itemsize], dtype=dt)
    return arr.reshape(a["count"], n) if n > 1 else arr
def mat_of(node):
    if "matrix" in node: return np.array(node["matrix"]).reshape(4, 4).T
    M = np.eye(4)
    if "scale" in node: M = np.diag(list(node["scale"]) + [1]) @ M
    if "rotation" in node:
        x, y, z, w = node["rotation"]
        R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)], [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)], [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
        R4 = np.eye(4); R4[:3, :3] = R; M = R4 @ M
    if "translation" in node: T = np.eye(4); T[:3, 3] = node["translation"]; M = T @ M
    return M
parts = {}
def walk(ni, M):
    nd = js["nodes"][ni]; W = M @ mat_of(nd)
    if "mesh" in nd:
        for p in js["meshes"][nd["mesh"]]["primitives"]:
            Pm = acc(p["attributes"]["POSITION"]).astype(np.float64); Nm = acc(p["attributes"]["NORMAL"]).astype(np.float64)
            Pw = (W[:3, :3] @ Pm.T).T + W[:3, 3]; Nw = (np.linalg.inv(W[:3, :3]).T @ Nm.T).T
            I = acc(p["indices"]).astype(np.int64) if "indices" in p else np.arange(len(Pm))
            parts.setdefault(p.get("material", 0), []).append((Pw, Nw, I))
    for c in nd.get("children", []): walk(c, W)
for n in js["scenes"][js.get("scene", 0)]["nodes"]: walk(n, np.eye(4))
allP = np.vstack([pp for v in parts.values() for pp, _, _ in v])
lo, hi = allP.min(0), allP.max(0); ctr = (lo + hi) / 2
k = .30 / (hi[1] - lo[1])                                      # 30 cm de alto
R = np.array([[0, 0, 1], [0, 1, 0], [-1, 0, 0]], np.float64)     # la cara (que miraba a -X) pasa a mirar a +Z
prims = []
for mi, lst in sorted(parts.items()):
    Pm = np.vstack([p for p, _, _ in lst]); Nm = np.vstack([n for _, n, _ in lst])
    I = []; o = 0
    for p, _, i in lst: I.append(i + o); o += len(p)
    I = np.concatenate(I)
    Pm = ((Pm - ctr) * k) @ R.T; Nm = Nm @ R.T
    name = js["materials"][mi]["name"] if "materials" in js else "m%d" % mi
    col = (.15, .15, .17, 1) if "age" in name.lower() else (.95, .95, .95, 1)
    prims.append(("Careta_" + ("Reja" if "age" in name.lower() else "Casco"), col, Pm, Nm, I))
write_glb(os.path.join(MOD, "SM_Careta.glb"), prims, extras={"author": "matýsek (https://sketchfab.com/matyprokop09)", "license": "CC-BY-4.0", "title": "Hockey Goalie Mask"})
