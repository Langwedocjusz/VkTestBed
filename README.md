# VkTestBed
A thin framework for impementing, testing and timing different graphical techniques with Vulkan API.

![alt text](https://github.com/Langwedocjusz/VkTestBed/blob/main/img/1.png?raw=true)

## Goals
The end goal is devloping an architecture that allows to efficiently specify a scene, and completely seprately a renderer.
It will then be possible to easily compare efficiency of different rendering techniques, even when they require completely different setup
(e.g. using one big buffer for vertex data vs multiple small ones).

## Current features

* Some (evolving) abstraction that makes it easier to interact with the Vulkan API
* Shader hot-reloading
* Imgui integration
* Tracy profiler integration for cpu timings
* Vulkan Queries for rough gpu timings
* Basic scene-graph implementation
* Loading textures form jpeg/png images using stb image
* Mipmap generation
* Loading gltf files (loading done with fastgltf, missing tangents re-generated with mikktspace)
* Converting equirectangular maps to cubemaps using compute pipeline
* Async Asset Loading

## Building
This repository contains submodules, so it should be cloned recursively:

	git clone --recursive https://github.com/Langwedocjusz/VkTestBed <TargetDir>

You can then use one of the bundled scripts to build the project:

* `Build.sh` is meant to be used on Linux, it ask for configuration (Release/Debug), generate makefiles and build the project
* `WinGenerateProjects.bat` is meant to be used on Windows, it will create Visual Studio solution files, which then need to be build using VS.

It is important to note that you should have Vulkan SDK installed and cmake installed and added to your PATH.

To download some assets (textures/models) used when developing this framework you can use the bundled script `scripts/DownloadAssets.py`.
To work it requires python3 and PyGithub.
