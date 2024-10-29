#!/usr/bin/env python3

import pathlib
import requests

def download_asset(url: str, destination: str):
    dest_path = pathlib.Path(destination)
    dest_path.mkdir(parents=True, exist_ok=True)

    filename = pathlib.Path(url).name

    with open(dest_path/filename, 'wb') as handle:
        response = requests.get(url, allow_redirects=True, headers={"User-Agent": "XY"})

        if not response.ok:
            print(response)

        file_data = response.content

        handle.write(file_data)

#Quad texture:
download_asset("https://vulkan-tutorial.com/images/texture.jpg", "./assets/textures")