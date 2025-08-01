#!/usr/bin/env python3

# This script adds diffuse transmission extension to all materials in a gltf
# file in an ad-hoc way. It assumes that all double-sided materials
# should have diffuse transmission enabled and calculates its color
# as average albedo color with increased saturation.
# This is done as a quick way to generate better looking foliage
# models, since at the moment gltf exporter in blender doesn't support the extension.

import json
from pathlib import Path
from argparse import ArgumentParser

import numpy as np
from PIL import Image, ImageEnhance

def process_material(material: dict) -> bool:
    if 'doubleSided' in material.keys():
        return material['doubleSided'] == True

    return False


def get_color(gltf: dict, material: dict, gltf_dir: Path) -> list:
    # To-do: handle non-texture, color-factor inputs
    tex_id = material['pbrMetallicRoughness']['baseColorTexture']['index']

    image_id = gltf['textures'][tex_id]['source']

    image_path = gltf_dir / gltf['images'][image_id]['uri']

    with Image.open(image_path) as img:
        converter = ImageEnhance.Color(img)
        img2 = converter.enhance(3.0)

        imarray = np.array(img2)

    alpha = imarray[:,:,3]

    r = np.mean(alpha * imarray[:,:,0]) / 255.0
    g = np.mean(alpha * imarray[:,:,1]) / 255.0
    b = np.mean(alpha * imarray[:,:,2]) / 255.0

    return [r,g,b]


def main():
    parser = ArgumentParser()
    parser.add_argument('filepath')

    args = parser.parse_args()

    filepath = Path(args.filepath)

    with open(filepath, 'r') as file:
        gltf = json.load(file)

    for material in gltf['materials']:
        if process_material(material):
            color = get_color(gltf, material, filepath.parent)
        
            material['extensions']['KHR_materials_diffuse_transmission'] = {
                'diffuseTransmissionFactor' : 1.0,
                'diffuseTransmissionColorFactor' : color
            }

    with open(filepath, 'w') as file:
        json.dump(gltf, file, indent=4)


if __name__ == "__main__":
    main()