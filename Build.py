#!/usr/bin/env python3

import subprocess

from argparse import ArgumentParser

def parse_args():
    compiler = "clang"

    parser = ArgumentParser()
    parser.add_argument('--gcc', action='store_true', help="Use gcc compiler, instead of default clang.")
    parser.add_argument('--nv', action='store_true', help="Disable Vulkan validation layers.")

    args = parser.parse_args()

    if args.gcc:
        compiler = "gcc"

    return compiler, args.nv

def get_build_type():
    options = ["Debug", "Release"]

    input_message = "Select build type:\n"

    for idx, opt in enumerate(options):
        input_message += f"{idx + 1}) {opt}\n"
    
    input_message += "Your choice: "

    user_input = ""

    while user_input not in {str(i+1) for i in range(len(options))}:
        user_input = input(input_message)

    return options[int(user_input) - 1]

def main():
    compiler, no_validation = parse_args()
    build_type = get_build_type()

    cmake_args = []

    if build_type == "Debug":
        cmake_args.append("-DCMAKE_BUILD_TYPE=Debug")
    elif build_type == "Release":
        cmake_args.append("-DCMAKE_BUILD_TYPE=Release")
    else:
        raise RuntimeError('Unsupported build type.')


    if compiler == "clang":
        cmake_args.append("-DCMAKE_C_COMPILER=clang")
        cmake_args.append("-DCMAKE_CXX_COMPILER=clang++")
    elif compiler == "gcc":
        cmake_args.append("-DCMAKE_C_COMPILER=gcc")
        cmake_args.append("-DCMAKE_CXX_COMPILER=g++")
    else:
        raise RuntimeError('Unsupported compiler.')

    if no_validation:
        cmake_args.append("-DUSE_VALIDATION_LAYERS=OFF")
    else:
        cmake_args.append("-DUSE_VALIDATION_LAYERS=ON")

    cmake_args = cmake_args + ["-G", "Ninja"]

    cmake_cmd = ["cmake"] + cmake_args + ["-S", ".", "-B", "./build"]

    subprocess.run(cmake_cmd)
    subprocess.run("(cd build && cmake --build .)", shell=True) 

if __name__ == "__main__":
    main()