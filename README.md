# VkTestBed
A thin framework for impementing, testing and timing different graphical techniques with the Vulkan API.
This is my personal pet engine. Used to learn about various graphical algorithms and compare their quality/performance.

![alt text](https://github.com/Langwedocjusz/VkTestBed/blob/main/img/3.png?raw=true)

## Building
It is important to note that you should have [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) and [cmake](https://cmake.org/) installed and added to your PATH.

This repository contains submodules, so it should be cloned recursively:

	git clone --recursive https://github.com/Langwedocjusz/VkTestBed <TargetDir>

You can then use one of the bundled scripts to build the project:

* `Build.sh` is meant to be used on Linux, it asks for configuration (Release/Debug), generates ninja files and builds the project
* `WinGenerateProjects.bat` is meant to be used on Windows, it will create Visual Studio solution files, which then need to be build using VS.

To download some assets (textures/models) used when developing this framework you can use the bundled script `scripts/DownloadAssets.py`.
To work it requires [python3](https://www.python.org/) and [PyGithub](https://pypi.org/project/PyGithub/). Auxiliary scripts that can compress gltf-referenced textures to BC7 encoded ktx files 
([compressonatorcli](https://github.com/GPUOpen-Tools/compressonator) must be in your PATH) and heuristically add diffuse translucency to materials are also provided there.

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
  	* Basic bilateral upsampler.
  	  
* PostFX:
  	* Physically Based Bloom, based on Jimenez approach, implemented with compute shaders.
  	* ACES Tonemapping.
  	  
* UI:
  	* Pixel perfect mouse picking, done by rendering object ids into 1x1 render target.
  	* Outline rendering.

Sources for most used techniques are documented in-place inside code comments.


