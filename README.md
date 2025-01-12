# Yet Another Renderer

## How to build
Launch setup.bat that will install all dependecies and init all used submodules

## What have already been done
- Abstract render layer
- OpenGL render backend
    - hlsl shaders support (precompile it to spir-v with dxc and make glsl from it using spriv-cross)
    - Shader reflection
    - Model loading using assimp
    - Basic light using point, directional and spot light
    - Materials (not PBR yet)
- OpenGL compute shader raytracer:
    - Lambertian, Metal and Dielectric materials support
- UI layer with ImGUI

## Plans
Here I will note what I'd like to do maybe in details (or not)  
I would like to start from the beginning ~~again~~ it means from OpenGL. Despite the fact it is pretty old GAPI  
I still think that it is a good place to refresh basics in my head  
**What I'd like to do**:  
- OpenGL renderer 
    - [x] Render 2D/3D objects
    - [x] Basic light 
    - [ ] Effects
    - [ ] Shadows
    - [ ] Some other render techniques 
- Render layer
    - [x] Shader reflection (HLSL -> Sprir-v -> Renderer)
    - [ ] Render Abstraction
        - [x] Basic abstraction (Buffers, Textures, Cmd Buffers, Descriptors)
        - [ ] More detailed Graphics Pipeline (now it's just has shader in it)
        - [ ] More detailed Compute Pipeline
- GUI (probably with ImGui)
    - [ ] Debug windows for Render targets output
    - [ ] Runtime shader recompilation
- Something else

## Dependencies
- Python 3.10
- Assimp
- dxc
- glad
- glfw
- glm
- imgui
- spirv-cross
- stb-image

## Links
- Basic OpenGL stuff - [LearnOpenGL](https://learnopengl.com/)
- Render abstraction reference - [The-Forge](https://github.com/ConfettiFX/The-Forge)
- Raytracer reference - [Raytracing in One Weekend](https://raytracing.github.io/)

