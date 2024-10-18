#!/usr/bin/env python3

import pathlib
import subprocess

DIRECTORIES = ["src"]

for dir in DIRECTORIES:
    headers = list(pathlib.Path(dir).rglob('*.h'))
    sources = list(pathlib.Path(dir).rglob('*.cpp'))

    files = headers + sources

    for file in files:
        subprocess.run(["clang-format", "-i", file])


