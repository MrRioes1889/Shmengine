# Shmengine (on hold)
<p align="center">
  <img width="45%" alt="sponza-capture" src="https://github.com/user-attachments/assets/4ba54eb2-7590-40a3-8ff6-d3abd53d7e07" />
  <img width="45%" alt="falchon_capture" src="https://github.com/user-attachments/assets/242fbc11-c5d0-4972-8876-0192dfa10743" />
</p>

## About

<b>Disclaimer: This solo project is created for educational/recreational purposes exclusively and is subject to constant restructuring. Hence, most parts of the engine are not "fully featured" and some older codepaths do contain less well thought out solutions compared to newer ones. Currently put on hold for a redo, built using the Odin programming language!</b><br><br>
This is an attempt at a high-performance 3D graphics engine with reasonably minimal third-party and standard-library dependencies.<br>
Platform and lib specific code is strictly abstracted into small sections for keeping the codebase as dependency-agnostic as possible.<br>
The general entry point is located in "Application/", the engine core in the one called "Shmengine/".<br>
Everything else, including application logic is organized in "modules/".

### External Dependencies
- Vulkan SDK
- Optick (Profiling)
- stb_image (png parsing)
- stb_truetype (ttf parsing)
### Features implemented fully or rudimentarily:
- Keyboard/Mouse input handling and keymaps
- Logging
- Vulkan Renderer Backend
- Application dll hot reloading
- Custom allocators
- Job system
- Text rendering (bitmap and static truetype)
- Scene object picking (via raycast and/or separate shaded framebuffer)
- Obj file parsing
- Basic textured 3D mesh rendering
- Directional/Point Lighting (Phong)
- Asynchronous mesh loading <b>(under reconstruction)</b>
- Terrain rendering
- Skybox rendering
- Custom text and binary file formats for scenes, shaders, meshes etc.
- Various utilities/data structures for strings, containers and linear algebra

## System Requirements
<b>Currently, the only supported platform is Windows</b>. Although adding other platforms should be fairly straight forward.<br>
Builds were done on Windows x64 systems using msvc as compiler with relatively modern dedicated GPUs (GTX 1080ti and RTX 3080) exclusively so far.<br>
Vulkan support is mandatory since it is the only renderer backend implemented at the moment.<br>

## How to build
Before building the [Vulkan SDK](https://vulkan.lunarg.com/) needs to be installed. The VULKAN_SDK system-wide macro should lead to the SDK's installation directory afterwards.<br> 
A script for the meta build tool Premake 5 is located in the "build-tools" directory. Assuming the premake5 runtime is registered in the system's PATH, the build.bat file can be run for building a Visual Studio solution.<br>
The code is separated into modules that largely get built into dynamic libraries to allow runtime backend swapping and hot-reloading of application-side code.

## Usage
Controls of Sandbox application (default):
| Action                          | Keys                  |
| :------------------------------ | :-------------------- |
| Camera control                  | arrow keys            |
| Clip cursor (mouse camera)      | C                     |
| Load scene                      | L                     |
| Unload scene                    | U                     |
| Change render mode              | ctrl + (1,2,3)        |

<b>For all implemented keybinds see modules/app/A_Sandbox/sauce/Keybinds.cpp</b>
