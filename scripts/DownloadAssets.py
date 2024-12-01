#!/usr/bin/env python3

import pathlib
import requests

from github import Github, Repository, ContentFile

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


def download_git_dir(repo: str, subdir: str, destination: str):
    dir_contents = Github().get_repo(repo).get_contents(subdir)

    def download(c: ContentFile, subdir: str, destination: str):
        trimmed_path = pathlib.Path(c.path.replace(subdir, ""))
        trimmed_path = trimmed_path.parent

        final_path = f'{destination}/{str(trimmed_path)}'

        download_asset(c.download_url, final_path)

    for c in dir_contents:
        if c.download_url is None:
            download_git_dir(repo, c.path, destination)
            continue

        download(c, subdir, destination)


#Quad test texture:
download_asset("https://vulkan-tutorial.com/images/texture.jpg", "./assets/textures")

#Damaged helmet GLTF:
download_asset("https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Models/refs/heads/main/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf", "./assets/gltf/DamagedHelmet")
download_asset("https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Models/refs/heads/main/2.0/DamagedHelmet/glTF/DamagedHelmet.bin", "./assets/gltf/DamagedHelmet")
download_asset("https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Models/refs/heads/main/2.0/DamagedHelmet/glTF/Default_albedo.jpg", "./assets/gltf/DamagedHelmet")

#Sponza GLTF:
download_git_dir("KhronosGroup/glTF-Sample-Models", "2.0/Sponza", "./assets/gltf/Sponza")