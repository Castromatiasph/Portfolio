"""Saca las texturas y las formas de cara (morph targets) del GLB de Matías para importar solo la malla con esqueleto
(~2 MB en lugar de 28 MB). Las texturas vestidas las genera Tools/dress_matias.py y se asignan desde el juego.
Uso: python Tools/prepare_matias_mesh.py"""
import json, struct, os, copy

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
