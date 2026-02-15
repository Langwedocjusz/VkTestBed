#!/usr/bin/env python3

import os
import json
import shutil
import subprocess

from pathlib import Path
from argparse import ArgumentParser

def main():
    # Parse user args:
    parser = ArgumentParser()

    parser.add_argument('-i', '--input', required=True, 
                        help="Path to the gltf file to process")
    parser.add_argument('-o', '--output', required=True, 
                        help="Path to the directory where processed gltf will be output.")

    args = parser.parse_args()

    in_gltf_path = Path(args.input)
    out_dir = Path(args.output)

    # Make output dir if it doesn't exist:
    os.makedirs(out_dir, exist_ok=True)

    # Load the original gltf json:
    with open(in_gltf_path, 'r') as file:
        gltf = json.load(file)

    # Copy binary buffers to new directory:
    for elem in gltf['buffers']:
        org_path = in_gltf_path.parent / elem['uri']
        new_path = out_dir / elem['uri']

        shutil.copy(org_path, new_path)

    # Compress all referenced images
    # Requires AMD compressontaor cli being present in PATH
    for image in gltf['images']:
        org_uri = image['uri']
        new_uri = str(Path(org_uri).with_suffix(".ktx"))

        org_path = in_gltf_path.parent / org_uri
        new_path = out_dir / new_uri

        subprocess.call([
            f'compressonatorcli -fd BC7 {org_path} {new_path}'
        ], shell=True)

    # Save gltf json with updated image URIs:
    with open(out_dir / in_gltf_path.name, 'w') as file:
        json.dump(gltf, file, indent=4)


if __name__ == "__main__":
    main()
