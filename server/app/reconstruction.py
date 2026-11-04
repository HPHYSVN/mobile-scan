"""Photogrammetry pipeline: images -> SfM (COLMAP via pycolmap) -> point
cloud -> Poisson surface (Open3D) -> cleaned, decimated GLB (trimesh).

Design notes (honest limits):
- Only the sparse SfM cloud is used; there is no dense MVS step, so the
  Poisson surface is a coarse approximation, best for roughly convex /
  well-textured subjects shot from many angles. This is a real pipeline,
  not a mock: every stage can fail and failures are reported as job errors.
- Images are downscaled to MAX_IMAGE_DIM before SfM for CPU feasibility.
"""

from __future__ import annotations

import shutil
import time
from pathlib import Path
from typing import Callable

import numpy as np

MAX_IMAGE_DIM = 1600
TARGET_FACES = 120_000
MIN_IMAGES = 8

Progress = Callable[[str, float], None]


def _log(progress: Progress, stage: str, frac: float, msg: str = "") -> None:
    progress(stage, frac)
    print(f"[{stage} {frac:.0%}] {msg}", flush=True)


def _prepare_images(image_dir: Path, work_dir: Path, progress: Progress) -> Path:
    from PIL import Image

    prepared = work_dir / "images_prepared"
    prepared.mkdir(parents=True, exist_ok=True)
    sources = sorted(
        p for p in image_dir.iterdir()
        if p.suffix.lower() in {".jpg", ".jpeg", ".png"}
    )
    if len(sources) < MIN_IMAGES:
        raise RuntimeError(
            f"need at least {MIN_IMAGES} images, got {len(sources)}"
        )
    for i, src in enumerate(sources):
        dst = prepared / src.name
        if dst.exists():
            continue
        with Image.open(src) as im:
            im = im.convert("RGB")
            if max(im.size) > MAX_IMAGE_DIM:
                im.thumbnail((MAX_IMAGE_DIM, MAX_IMAGE_DIM))
            im.save(dst, quality=92)
        if i % 10 == 0:
            _log(progress, "prepare", 0.05 * i / len(sources), f"{i}/{len(sources)}")
    return prepared


def _run_sfm(image_dir: Path, work_dir: Path, progress: Progress):
    import pycolmap

    database = work_dir / "database.db"
    sparse_dir = work_dir / "sparse"
    sparse_dir.mkdir(parents=True, exist_ok=True)
    if database.exists():
        database.unlink()

    _log(progress, "sfm", 0.1, "extracting SIFT features (CPU)")
    extraction = pycolmap.FeatureExtractionOptions()
    extraction.sift.max_num_features = 8192
    pycolmap.extract_features(
        database_path=str(database),
        image_path=str(image_dir),
        camera_mode=pycolmap.CameraMode.SINGLE,
        extraction_options=extraction,
    )

    _log(progress, "sfm", 0.35, "exhaustive matching")
    pycolmap.match_exhaustive(database_path=str(database))

    _log(progress, "sfm", 0.55, "incremental mapping")
    reconstructions = pycolmap.incremental_mapping(
        database_path=str(database),
        image_path=str(image_dir),
        output_path=str(sparse_dir),
    )
    if not reconstructions:
        raise RuntimeError("SfM failed: COLMAP registered no reconstruction")

    # Pick the reconstruction with the most registered images / points.
    best = max(
        reconstructions.values(),
        key=lambda r: (r.num_reg_images(), r.num_points3D()),
    )
    if best.num_reg_images() < MIN_IMAGES:
        raise RuntimeError(
            f"SfM registered only {best.num_reg_images()} images "
            f"(need {MIN_IMAGES}+). Shoot more overlap / better light."
        )
    _log(
        progress, "sfm", 0.7,
        f"registered {best.num_reg_images()} images, {best.num_points3D()} points",
    )
    return best


def _points_and_cameras(reconstruction):
    points = np.array(
        [p.xyz for p in reconstruction.points3D.values()], dtype=np.float64
    )
    colors = np.array(
        [p.color for p in reconstruction.points3D.values()], dtype=np.float64
    ) / 255.0
    centers = []
    for image in reconstruction.images.values():
        cam = image.cam_from_world()
        centers.append(-cam.rotation.matrix().T @ cam.translation)
    camera_center = np.mean(centers, axis=0) if centers else np.zeros(3)
    return points, colors, camera_center


def _mesh_from_points(points, colors, camera_center, progress: Progress) -> Path:
    import open3d as o3d

    _log(progress, "meshing", 0.75, f"Poisson on {len(points)} points")
    pcd = o3d.geometry.PointCloud()
    pcd.points = o3d.utility.Vector3dVector(points)
    if len(colors) == len(points):
        pcd.colors = o3d.utility.Vector3dVector(np.clip(colors, 0.0, 1.0))
    pcd.estimate_normals(
        search_param=o3d.geometry.KDTreeSearchParamKNN(knn=20)
    )
    pcd.orient_normals_towards_camera_location(camera_center)

    mesh, densities = o3d.geometry.TriangleMesh.create_from_point_cloud_poisson(
        pcd, depth=8, scale=1.05
    )
    densities = np.asarray(densities)
    if len(densities) > 0 and len(mesh.vertices) > 0:
        # Trim the lowest-density (least supported) surface, as in the
        # official Open3D Poisson tutorial.
        keep = densities > np.quantile(densities, 0.05)
        mesh.remove_vertices_by_mask(~keep)
    mesh.remove_duplicated_vertices()
    mesh.remove_unreferenced_vertices()
    if len(mesh.triangles) == 0:
        raise RuntimeError("meshing failed: Poisson produced no triangles")
    _log(progress, "meshing", 0.85, f"{len(mesh.triangles)} faces before decimation")
    return mesh


def _export_glb(mesh, out_path: Path, progress: Progress) -> dict:
    import trimesh

    _log(progress, "export", 0.9, "decimating + exporting GLB")
    tri = trimesh.Trimesh(
        vertices=np.asarray(mesh.vertices),
        faces=np.asarray(mesh.triangles),
        vertex_colors=np.asarray(mesh.vertex_colors)
        if mesh.has_vertex_colors() else None,
        process=False,
    )
    if len(tri.faces) > TARGET_FACES:
        try:
            tri = tri.simplify_quadric_decimation(face_count=TARGET_FACES)
        except Exception as exc:  # decimation is an optimisation, not a blocker
            print(f"simplify skipped: {exc}", flush=True)
    tri.export(out_path, file_type="glb")
    return {
        "vertices": int(len(tri.vertices)),
        "faces": int(len(tri.faces)),
        "bytes": out_path.stat().st_size,
    }


def reconstruct(session_dir: Path, progress: Progress) -> dict:
    """Runs the full pipeline for one session directory. Raises on failure."""
    started = time.time()
    image_dir = session_dir / "images"
    work_dir = session_dir / "work"
    if work_dir.exists():
        shutil.rmtree(work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    prepared = _prepare_images(image_dir, work_dir, progress)
    reconstruction = _run_sfm(prepared, work_dir, progress)
    points, colors, camera_center = _points_and_cameras(reconstruction)
    mesh = _mesh_from_points(points, colors, camera_center, progress)
    out_path = session_dir / "result.glb"
    stats = _export_glb(mesh, out_path, progress)
    stats.update({
        "registered_images": int(reconstruction.num_reg_images()),
        "sparse_points": int(reconstruction.num_points3D()),
        "seconds": round(time.time() - started, 1),
    })
    _log(progress, "done", 1.0, str(stats))
    return stats
