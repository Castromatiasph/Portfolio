"""Saca las texturas y las formas de cara (morph targets) del GLB de Matías para importar solo la malla con esqueleto
(~2 MB en lugar de 28 MB). Las texturas vestidas las genera Tools/dress_matias.py y se asignan desde el juego.
Uso: python Tools/prepare_matias_mesh.py"""
import json, struct, os, copy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "SourceAssets", "Models", "Matias_175cm.glb")
DST = os.path.join(HERE, "..", "SourceAssets", "Models", "SK_Matias.glb")

b = open(SRC, "rb").read()
off = 12; js = None; bo = None
while off < len(b):
    cl, ct = struct.unpack("<I4s", b[off:off + 8])
    if ct == b"JSON": js = json.loads(b[off + 8:off + 8 + cl])
    elif ct == b"BIN\0": bo = off + 8
    off += 8 + cl

# 0) "hornear" los morph targets con sus pesos por defecto (cara de Matías y forma del cuerpo): sin esto la cara queda deforme
b = bytearray(b)
def raw(v, off0, count, comp, dt):
    v = js["bufferViews"][v]; off = bo + v.get("byteOffset", 0) + off0
    return np.frombuffer(bytes(b[off:off + count * comp * np.dtype(dt).itemsize]), dtype=dt).reshape(count, comp) if comp > 1 else \
        np.frombuffer(bytes(b[off:off + count * np.dtype(dt).itemsize]), dtype=dt)
def read3(acc):
    a = js["accessors"][acc]
    if "bufferView" in a:
        v = js["bufferViews"][a["bufferView"]]
        st = v.get("byteStride", 12); off = bo + v.get("byteOffset", 0) + a.get("byteOffset", 0)
        out = np.stack([np.frombuffer(bytes(b[off + i * st:off + i * st + 12]), dtype="<f4") for i in range(a["count"])]).astype(np.float64)
    else:
        out, off, st = np.zeros((a["count"], 3)), 0, 12
    sp = a.get("sparse")
    if sp:                                                              # accessor "disperso": solo trae los vértices que cambian
        ix, vl = sp["indices"], sp["values"]
        idx = raw(ix["bufferView"], ix.get("byteOffset", 0), sp["count"], 1, {5121: "u1", 5123: "<u2", 5125: "<u4"}[ix["componentType"]]).astype(np.int64)
        val = raw(vl["bufferView"], vl.get("byteOffset", 0), sp["count"], 3, "<f4").astype(np.float64)
        out[idx] = val
    return out, off, st
def write3(acc, arr, off, st):
    for i in range(len(arr)): b[off + i * st:off + i * st + 12] = arr[i].astype("<f4").tobytes()
for m in js["meshes"]:
    w = m.get("weights") or []
    for p in m["primitives"]:
        tg = p.get("targets")
        if not tg or not w: continue
        P, po, ps = read3(p["attributes"]["POSITION"])
        N, no, ns = read3(p["attributes"]["NORMAL"]) if "NORMAL" in p["attributes"] else (None, 0, 0)
        for wi, t in zip(w, tg):
            if abs(wi) < 1e-6: continue
            if "POSITION" in t: P += wi * read3(t["POSITION"])[0]
            if N is not None and "NORMAL" in t: N += wi * read3(t["NORMAL"])[0]
        write3(p["attributes"]["POSITION"], P, po, ps)
        acc = js["accessors"][p["attributes"]["POSITION"]]; acc["min"] = P.min(0).tolist(); acc["max"] = P.max(0).tolist()
        if N is not None:
            N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
            write3(p["attributes"]["NORMAL"], N, no, ns)
        print("morphs horneados:", m["name"], len(tg), "targets")

# 0b) el cuerpo trae pegada una "túnica" (malla de ropa de ~740 vértices que cubre las piernas): se la saca para que se vean las piernas desnudas
def drop_robe(p):
    pa = js["accessors"][p["attributes"]["POSITION"]]
    P, _, _ = read3(p["attributes"]["POSITION"])
    ia = js["accessors"][p["indices"]]; iv = js["bufferViews"][ia["bufferView"]]
    dt = {5123: "<u2", 5125: "<u4"}[ia["componentType"]]; ioff = bo + iv.get("byteOffset", 0) + ia.get("byteOffset", 0)
    I = np.frombuffer(bytes(b[ioff:ioff + ia["count"] * np.dtype(dt).itemsize]), dtype=dt).reshape(-1, 3).astype(np.int64)
    par = np.arange(len(P))
    def find(x):
        while par[x] != x: par[x] = par[par[x]]; x = par[x]
        return x
    for t in I:
        r = find(t[0])
        for v in t[1:]: par[find(v)] = r
    roots = np.array([find(i) for i in range(len(P))])
    keep = np.ones(len(I), bool); dropped = 0
    for r in np.unique(roots[np.unique(I)]):
        m = roots == r
        y0, y1 = P[m, 1].min(), P[m, 1].max(); xw = P[m, 0].max() - P[m, 0].min()
        if m.sum() > 300 and 0.05 < y0 < 0.25 and 0.8 < y1 < 1.25 and xw > 0.4:       # largo de rodilla a cintura y ancho de cadera: la túnica
            keep &= ~m[I[:, 0]]; dropped += 1
        elif m.sum() <= 70:                                               # tiras de "pelo", bolitas junto a los ojos y piezas sueltas pegadas al cuerpo
            keep &= ~m[I[:, 0]]; dropped += 1
    out = I[keep].astype(dt).ravel()
    b[ioff:ioff + out.nbytes] = out.tobytes()
    ia["count"] = int(out.size)
    print("túnica: componentes quitados =", dropped, " triángulos", len(I), "->", int(keep.sum()))
for m in js["meshes"]:
    for p in m["primitives"]:
        if m["name"] == "base": drop_robe(p)

# 1) quitar morph targets
for m in js["meshes"]:
    for p in m["primitives"]: p.pop("targets", None)
    m.pop("weights", None)
    if "extras" in m: m["extras"].pop("targetNames", None)
for n in js["nodes"]: n.pop("weights", None)
# 2) quitar texturas y sus referencias en los materiales
for k in ("images", "textures", "samplers"): js.pop(k, None)
def strip(o):
    if isinstance(o, dict):
        for k in list(o.keys()):
            if k.endswith("Texture"): del o[k]
            else: strip(o[k])
    elif isinstance(o, list):
        for x in o: strip(x)
for m in js["materials"]:
    strip(m); m["alphaMode"] = "OPAQUE"; m.pop("extensions", None)
js.pop("extensionsUsed", None); js.pop("extensionsRequired", None)
# 3) quedarse solo con los bufferViews que usan los accessors restantes
used = sorted({a["bufferView"] for a in js["accessors"] if "bufferView" in a})
# accessors que solo usaban los morph targets quedan huérfanos: los descartamos
ref = set()
for m in js["meshes"]:
    for p in m["primitives"]:
        ref.update(p["attributes"].values())
        if "indices" in p: ref.add(p["indices"])
for s in js.get("skins", []):
    if "inverseBindMatrices" in s: ref.add(s["inverseBindMatrices"])
for a in js.get("animations", []):
    for smp in a["samplers"]: ref.update([smp["input"], smp["output"]])
amap = {old: new for new, old in enumerate(sorted(ref))}
accs = [js["accessors"][o] for o in sorted(ref)]
bvs_used = sorted({a["bufferView"] for a in accs if "bufferView" in a})
bvmap = {old: new for new, old in enumerate(bvs_used)}
newbin = bytearray(); newbvs = []
for old in bvs_used:
    v = copy.deepcopy(js["bufferViews"][old]); s = bo + v.get("byteOffset", 0)
    while len(newbin) % 4: newbin.append(0)
    v["byteOffset"] = len(newbin); newbin += b[s:s + v["byteLength"]]; newbvs.append(v)
for a in accs: a["bufferView"] = bvmap[a["bufferView"]]
js["accessors"] = accs; js["bufferViews"] = newbvs
for m in js["meshes"]:
    for p in m["primitives"]:
        p["attributes"] = {k: amap[v] for k, v in p["attributes"].items()}
        if "indices" in p: p["indices"] = amap[p["indices"]]
for s in js.get("skins", []):
    if "inverseBindMatrices" in s: s["inverseBindMatrices"] = amap[s["inverseBindMatrices"]]
js["buffers"] = [{"byteLength": len(newbin)}]
j = json.dumps(js, separators=(",", ":")).encode()
while len(j) % 4: j += b" "
while len(newbin) % 4: newbin.append(0)
out = struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(j) + 8 + len(newbin)) + struct.pack("<I4s", len(j), b"JSON") + j + struct.pack("<I4s", len(newbin), b"BIN\0") + bytes(newbin)
open(DST, "wb").write(out)
print("ok", DST, len(out), "bytes")
