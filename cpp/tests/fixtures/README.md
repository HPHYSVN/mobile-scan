# Benchmark fixtures

Generated deterministically by `scripts/generate_fixtures.py` (no external assets, no license encumbrance — geometry is generated in-repo):

| File | Verts | Tris | Bytes |
|---|---|---|---|
| sphere_6k.glb | 3,185 | 6,144 | 176,472 |
| sphere_24k.glb | 12,513 | 24,576 | 696,160 |
| sphere_98k.glb | 49,601 | 98,304 | 2,767,716 |
| sphere_393k.glb | 197,505 | 393,216 | 11,039,600 |

Regenerate: `python3 scripts/generate_fixtures.py cpp/tests/fixtures`
The `.glb` files in this directory are build artifacts and are git-ignored; copies of the 24k/98k fixtures live in `app/src/main/assets/` for the Android validation build.
