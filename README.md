# VkTestBed
A thin framework for impementing, testing and timing different graphical techniques with Vulkan API.

![alt text](https://github.com/Langwedocjusz/VkTestBed/blob/main/img/3.png?raw=true)

## Goals
This is my personal pet engine. Used to learn about various graphical algorithms and compare their quality/performance.

The original goal was devloping an architecture that allows to efficiently specify a scene, and completely seprately a renderer,
to facilitate easy comparison of differnet techniques - all from gui, without recompiling/changing source code.
The idea was to set up as such comparisons as possible, but combinatorial explosion of different codepaths quickly
made the idea of maintaining all that unrealistic, so I decided to limit the scope to just implementing interesting techniques,
and keep the comparison aspect in the raw form.

## Current features

* General
	* Some (evolving) abstraction that makes it easier to interact with the Vulkan API
	* Live shader hot-reloading
	* Imgui integration
	* Tracy profiler integration for cpu timings
	* Vulkan Queries setup for rough gpu timings
	* Basic scene-graph implementation
    * Asynchronous, multithreaded asset loadng (gltf files, textures, envmaps).
    * Support for compressed textures.

* Base Renderer:
	* Physically Based Rendering with Roughness-Metalness workflow.
 	* Includes IBL, where spherical harmonics (diffuse) and prefiltered maps (specular) are derived from input equirectangular map.
 	* Translucency support (works great on foliage).
  	* Basic alpha blended transparency support.	 
	* State of the Art Vertex Compression (Quantization + Octahedral Map + Rodriguez Rotation).
   	* Frustum Culling and Z-Prepass Optimizations.
   	* HDR render target.
   	* Full support for MSAA antialiasing.

* Shadows:
	* Cascaded Shadow Mapping
    * Implementation of both z and normal bias.
   	* PCF filtering for soft shadows.

* Screen Space Ambient Occlusion:
  	* Base kernel based on Volumetric Obscurance and Alchemy AO papers (subject to change).
  	* Uses imporved normal reconstruction from depth.
  	* Resolution decoupled from main render target.
  	* Basic bilinear upsampler.

* PostFX:
  	* Physically Based Bloom, based on Jimenez approach, implemented with compute shaders.
  	* ACES Tonemapping.
  	* 
* UI:
  	* Pixel perfect mouse picking, done by rendering object ids into 1x1 render target.
  	* Outline rendering.

Sources for most used techniques are documented in-place inside code comments.

## Building
It is important to note that you should have Vulkan SDK installed and cmake installed and added to your PATH.

This repository contains submodules, so it should be cloned recursively:

	git clone --recursive https://github.com/Langwedocjusz/VkTestBed <TargetDir>

You can then use one of the bundled scripts to build the project:

* `Build.sh` is meant to be used on Linux, it ask for configuration (Release/Debug), generate makefiles and build the project
* `WinGenerateProjects.bat` is meant to be used on Windows, it will create Visual Studio solution files, which then need to be build using VS.

To download some assets (textures/models) used when developing this framework you can use the bundled script `scripts/DownloadAssets.py`.
To work it requires python3 and PyGithub.
