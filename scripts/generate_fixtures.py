#!/usr/bin/env python3
"""Generate UV-sphere GLB fixtures for import/render benchmarks.

Deterministic, no external dependencies. Output meshes have POSITION,
NORMAL, TEXCOORD_0 and uint32 indices in a single primitive.
Usage: generate_fixtures.py <out_dir>
"""
import json
import math
import struct
import sys
from pathlib import Path


def sphere(lon: int, lat: int):
    positions, normals, uvs, indices = [], [], [], []
    for j in range(lat + 1):
        v = j / lat
        phi = v * math.pi
        for i in range(lon + 1):
            u = i / lon
            theta = u * 2.0 * math.pi
            x = math.sin(phi) * math.cos(theta)
            y = math.cos(phi)
            z = math.sin(phi) * math.sin(theta)
            positions += [x, y, z]
            normals += [x, y, z]
            uvs += [u, v]
    for j in range(lat):
        for i in range(lon):
            a = j * (lon + 1) + i
            b = a + lon + 1
            indices += [a, b, a + 1, b, b + 1, a + 1]
    return positions, normals, uvs, indices


def write_glb(path: Path, lon: int, lat: int) -> int:
    pos, nrm, uv, idx = sphere(lon, lat)
    bin_data = b"".join([
        struct.pack(f"<{len(pos)}f", *pos),
        struct.pack(f"<{len(nrm)}f", *nrm),
        struct.pack(f"<{len(uv)}f", *uv),
        struct.pack(f"<{len(idx)}I", *idx),
    ])
    off = 0
    views = []
    for size in (len(pos) * 4, len(nrm) * 4, len(uv) * 4, len(idx) * 4):
        views.append({"buffer": 0, "byteOffset": off, "byteLength": size})
        off += size
    xs, ys, zs = pos[0::3], pos[1::3], pos[2::3]
    gltf = {
        "asset": {"version": "2.0", "generator": "mobile3d generate_fixtures.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3}]}],
        "buffers": [{"byteLength": len(bin_data)}],
        "bufferViews": views,
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(pos) // 3, "type": "VEC3",
             "min": [min(xs), min(ys), min(zs)], "max": [max(xs), max(ys), max(zs)]},
            {"bufferView": 1, "componentType": 5126, "count": len(nrm) // 3, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(uv) // 2, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5125, "count": len(idx), "type": "SCALAR"},
        ],
    }
    js = json.dumps(gltf, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    bin_padded = bin_data + b"\0" * ((4 - len(bin_data) % 4) % 4)
    total = 12 + 8 + len(js) + 8 + len(bin_padded)
    path.write_bytes(b"".join([
        struct.pack("<III", 0x46546C67, 2, total),
        struct.pack("<I4s", len(js), b"JSON"), js,
        struct.pack("<I4s", len(bin_padded), b"BIN\0"), bin_padded,
    ]))
    tris = len(idx) // 3
    print(f"{path.name}: verts={len(pos)//3} tris={tris} bytes={total}")
    return tris


def main() -> None:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "cpp/tests/fixtures")
    out.mkdir(parents=True, exist_ok=True)
    write_glb(out / "sphere_6k.glb", 64, 48)
    write_glb(out / "sphere_24k.glb", 128, 96)
    write_glb(out / "sphere_98k.glb", 256, 192)
    write_glb(out / "sphere_393k.glb", 512, 384)


if __name__ == "__main__":
    main()
