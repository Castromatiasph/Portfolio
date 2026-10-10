// Prepara el patín (gozdemrl, "70's - 80's Roller Skate", CC BY 4.0) para Unreal:
// descomprime meshopt + KTX2, baja las texturas a 512 px, orienta la punta hacia +Z (adelante en glTF), apoya las ruedas en y=0,
// centra cerca del tobillo y escala a 30 cm de largo. Uso: node prepare_skate.mjs entrada.glb salida.glb
// Dependencias: npm i @gltf-transform/core @gltf-transform/extensions meshoptimizer pngjs three  (y copiar three/examples/jsm/libs/basis/basis_transcoder.{js,wasm} como ./basis.cjs y ./basis_transcoder.wasm)
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { MeshoptDecoder } from 'meshoptimizer';
import { PNG } from 'pngjs';
import fs from 'fs';
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const BASIS = require('./basis.cjs');
const wasm = fs.readFileSync('./node_modules/three/examples/jsm/libs/basis/basis_transcoder.wasm');
const [,, inp, outp] = process.argv;
await MeshoptDecoder.ready;
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS).registerDependencies({ 'meshopt.decoder': MeshoptDecoder });
const doc = await io.read(inp);
const mod = await BASIS({ wasmBinary: wasm }); mod.initializeBasis();

function half(png) {            // 1024 -> 512 (promedio de 2x2)
  const w = png.width >> 1, h = png.height >> 1, out = new PNG({ width: w, height: h });
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) for (let c = 0; c < 4; c++) {
    let s = 0; for (let dy = 0; dy < 2; dy++) for (let dx = 0; dx < 2; dx++) s += png.data[(((y * 2 + dy) * png.width) + x * 2 + dx) * 4 + c];
    out.data[(y * w + x) * 4 + c] = s >> 2;
  }
  return out;
}
for (const tex of doc.getRoot().listTextures()) {
  if (tex.getMimeType() !== 'image/ktx2') continue;
  const f = new mod.KTX2File(new Uint8Array(tex.getImage()));
  const w = f.getWidth(), h = f.getHeight(); f.startTranscoding();
  const dst = new Uint8Array(f.getImageTranscodedSizeInBytes(0, 0, 0, 13));
  f.transcodeImage(dst, 0, 0, 0, 13, 0, -1, -1);
  let png = new PNG({ width: w, height: h }); png.data = Buffer.from(dst.buffer, dst.byteOffset, w * h * 4);
  if (w >= 1024) png = half(png);
  tex.setImage(new Uint8Array(PNG.sync.write(png))).setMimeType('image/png'); f.close(); f.delete();
}
for (const ext of doc.getRoot().listExtensionsUsed()) if (['KHR_texture_basisu', 'EXT_meshopt_compression'].includes(ext.extensionName)) ext.dispose();

// límites globales en el espacio original
let mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9];
const prims = [];
for (const mesh of doc.getRoot().listMeshes()) for (const p of mesh.listPrimitives()) prims.push(p);
for (const p of prims) { const a = p.getAttribute('POSITION'); const v = [0, 0, 0]; for (let i = 0; i < a.getCount(); i++) { a.getElement(i, v); for (let k = 0; k < 3; k++) { mn[k] = Math.min(mn[k], v[k]); mx[k] = Math.max(mx[k], v[k]); } } }
const s = 0.30 / (mx[0] - mn[0]), x0 = 20, z0 = (mn[2] + mx[2]) / 2, y0 = mn[1];
console.log('límites', mn, mx, 'escala', s);
// (x,y,z) -> (-(z - z0)s, (y - y0)s, (x - x0)s)  [rotación propia de -90° sobre Y: no invierte triángulos]
for (const p of prims) {
  const pos = p.getAttribute('POSITION'), nor = p.getAttribute('NORMAL'), tan = p.getAttribute('TANGENT');
  const v = [0, 0, 0];
  for (let i = 0; i < pos.getCount(); i++) { pos.getElement(i, v); pos.setElement(i, [-(v[2] - z0) * s, (v[1] - y0) * s, (v[0] - x0) * s]); }
  if (nor) for (let i = 0; i < nor.getCount(); i++) { nor.getElement(i, v); nor.setElement(i, [-v[2], v[1], v[0]]); }
  if (tan) { const t = [0, 0, 0, 0]; for (let i = 0; i < tan.getCount(); i++) { tan.getElement(i, t); tan.setElement(i, [-t[2], t[1], t[0], t[3]]); } }
}
await io.write(outp, doc);
console.log('ok', outp);
