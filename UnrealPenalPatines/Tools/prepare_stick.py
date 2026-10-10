"""Normaliza el palo (hockey_stick_original.glb, "Hockey Stick" de Yogensia, CC BY-NC-SA 4.0) para Unreal:
tapón arriba en el origen, caña hacia abajo (-Y de glTF), pala curvada hacia adelante (+Z de glTF), largo 1,23 m.
Uso: python Tools/prepare_stick.py"""
import json, struct, os
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "SourceAssets", "Models", "hockey_stick_original.glb")
DST = os.path.join(HERE, "..", "SourceAssets", "Models", "SM_Palo.glb")
LENGTH = 1.23

b = open(SRC, "rb").read()
off = 12; js = None; bo = None
while off < len(b):
    cl, ct = struct.unpack("<I4s", b[off:off + 8])
    if ct == b"JSON": js = json.loads(b[off + 8:off + 8 + cl])
    elif ct == b"BIN\0": bo = off + 8
    off += 8 + cl

def acc(i):
    a = js["accessors"][i]; bv = js["bufferViews"][a["bufferView"]]
    n = {"VEC3": 3, "VEC2": 2, "SCALAR": 1}[a["type"]]; dt = np.uint32 if a["componentType"] == 5125 else np.float32
    st = bv.get("byteStride") or 4 * n; base = bo + bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    return np.array([np.frombuffer(b[base + k * st:base + k * st + 4 * n], dtype=dt) for k in range(a["count"])]).reshape(a["count"], n)

pr = js["meshes"][0]["primitives"][0]
P = acc(pr["attributes"]["POSITION"]).astype(np.float64)
N = acc(pr["attributes"]["NORMAL"]).astype(np.float64)
UV = acc(pr["attributes"]["TEXCOORD_0"]).astype(np.float32)
IDX = acc(pr["indices"]).astype(np.uint32).reshape(-1)
M = np.array(js["nodes"][0]["matrix"]).reshape(4, 4).T[:3, :3]     # el nodo raíz gira el modelo (-90° en X)
P = (M @ P.T).T; N = (M @ N.T).T
# ahora: u = P[:,0] caña (mango en u mínimo, pala en u máximo), v = P[:,1] espesor, w = P[:,2] pala hacia -w
u, v, w = P[:, 0], P[:, 1], P[:, 2]
k = LENGTH / (u.max() - u.min())
shaft_w = np.median(w[u < u.min() + (u.max() - u.min()) * .3])      # eje de la caña en w
vmid = (v.max() + v.min()) / 2
# a ejes de glTF con el frente en +Z: x = -v, y = -(u - umin), z = -(w - eje)
Q = np.stack([-(v - vmid) * k, -(u - u.min()) * k, -(w - shaft_w) * k], axis=1).astype(np.float32)
QN = np.stack([-N[:, 1], -N[:, 0], -N[:, 2]], axis=1)
QN = (QN / np.linalg.norm(QN, axis=1, keepdims=True)).astype(np.float32)
IDX = IDX.reshape(-1, 3)[:, ::-1].reshape(-1)                         # la transformación es un espejo: se invierte el orden de los triángulos
print("largo", float(Q[:, 1].max() - Q[:, 1].min()), "ancho pala", float(Q[:, 2].max() - Q[:, 2].min()), "espesor", float(Q[:, 0].max() - Q[:, 0].min()))
print("bbox", Q.min(0), Q.max(0))

buf = bytearray()
def add(arr, target):
    while len(buf) % 4: buf.append(0)
    s = len(buf); buf.extend(arr.tobytes()); return s, len(arr.tobytes())
sp, lp = add(Q, 34962); sn, ln = add(QN, 34962); st, lt = add(UV, 34962); si, li = add(IDX, 34963)
g = {
    "asset": {"version": "2.0", "generator": "prepare_stick.py", "extras": {"author": "Yogensia (https://sketchfab.com/yogensia)", "license": "CC-BY-NC-SA-4.0", "title": "Hockey Stick"}},
    "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": "SM_Palo"}],
    "meshes": [{"name": "SM_Palo", "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0}]}],
    "materials": [{"name": "Madera", "pbrMetallicRoughness": {"baseColorFactor": [0.75, 0.55, 0.3, 1], "metallicFactor": 0, "roughnessFactor": 0.4}}],
    "buffers": [{"byteLength": len(buf)}],
    "bufferViews": [{"buffer": 0, "byteOffset": sp, "byteLength": lp, "target": 34962}, {"buffer": 0, "byteOffset": sn, "byteLength": ln, "target": 34962},
                    {"buffer": 0, "byteOffset": st, "byteLength": lt, "target": 34962}, {"buffer": 0, "byteOffset": si, "byteLength": li, "target": 34963}],
    "accessors": [{"bufferView": 0, "componentType": 5126, "count": len(Q), "type": "VEC3", "min": Q.min(0).tolist(), "max": Q.max(0).tolist()},
                  {"bufferView": 1, "componentType": 5126, "count": len(QN), "type": "VEC3"},
                  {"bufferView": 2, "componentType": 5126, "count": len(UV), "type": "VEC2"},
                  {"bufferView": 3, "componentType": 5125, "count": len(IDX), "type": "SCALAR"}],
}
j = json.dumps(g, separators=(",", ":")).encode()
while len(j) % 4: j += b" "
while len(buf) % 4: buf.append(0)
open(DST, "wb").write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(j) + 8 + len(buf)) + struct.pack("<I4s", len(j), b"JSON") + j + struct.pack("<I4s", len(buf), b"BIN\0") + bytes(buf))
print("ok", DST)
