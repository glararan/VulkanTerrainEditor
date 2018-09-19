# Vulkan Terrain Editor
3D editor using Vulkan API

# Visual Studio
- Install Qt then extension for VS
- Install LunarG then configure project to include "VulkanSDK/version/include" and "VulkanSDK/version/lib"

# Shader compilation (GLSL to SPIR-V)
- glslangvalidator -V terrain.vert -o terrain.vert.spv
- glslangvalidator -V terrain.frag -o terrain.frag.spv
- glslangvalidator -V skysphere.vert -o skysphere.vert.spv
- glslangvalidator -V skysphere.frag -o skysphere.frag.spv
- glslangvalidator -V terrain.tesc -o terrain.tesc.spv
- glslangvalidator -V terrain.tese -o terrain.tese.spv