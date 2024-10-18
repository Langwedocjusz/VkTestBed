#!/usr/bin/env python3

import pathlib
import subprocess

def get_files(dir: str, regex: str):
    return list(pathlib.Path(dir).rglob(regex))

SOURCE_DIR = "assets/shaders"
SPIRV_DIR = "assets/spirv"

result_dir = pathlib.Path(SPIRV_DIR)
result_dir.mkdir(parents=True, exist_ok=True)

verts = get_files(SOURCE_DIR, "*.vert")
frags = get_files(SOURCE_DIR, "*.frag")
comps = get_files(SOURCE_DIR, "*.comp")

filepaths = verts + frags + comps;

for path in filepaths:
    result_name = path.stem
    ext = path.suffix

    if ext == ".vert":
        result_name += "Vert.spv"
    elif ext == ".frag":
        result_name += "Frag.spv"
    elif ext == ".comp":
        result_name += "Comp.spv"

    result_path = result_dir / result_name

    subprocess.run(["glslc", path, "-o", result_path])