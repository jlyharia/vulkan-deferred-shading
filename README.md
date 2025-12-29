# Defer shading

### Run/Debug Configuration (CLion)
To enable validation layers and debug markers when using a local SDK install, you must manually point the loader to the SDK binaries.

1. Go to **Run > Edit Configurations**.
2. Add the following to **Environment variables**:
   ```text
   VK_LAYER_PATH=/home/johnny/VulkanSDK/1.4.335.0/x86_64/share/vulkan/explicit_layer.d
   LD_LIBRARY_PATH=/home/johnny/VulkanSDK/1.4.335.0/x86_64/lib


## Project Structure

This project follows a **Project-Base (Sibling)** architecture. Headers (`.hpp`) and source files (`.cpp`) are kept in the same directory to improve developer velocity and maintain clear module boundaries.

```text
defer_render/
├── CMakeLists.txt          # Build logic & Shader compilation rules
├── README.md               # Setup instructions & Environment variables
├── main.cpp                # App entry point & Main loop
├── shaders/                # GLSL source code (Vertex/Fragment/Compute)
│   ├── gbuffer.vert
│   ├── gbuffer.frag
│   ├── lighting.vert
│   └── lighting.frag
├── assets/                 # Runtime resources (Models/Textures)
│   ├── models/
│   └── textures/
└── src/                    # Source Code Root
    ├── platform/           # Windowing & OS abstraction
    │   ├── window.hpp
    │   └── window.cpp
    ├── vulkan/             # Low-level Vulkan wrappers (Toolbox)
    │   ├── context.hpp     # Instance/Device/Queues
    │   ├── context.cpp
    │   ├── swapchain.hpp   # Presentation & Surface management
    │   ├── swapchain.cpp
    │   ├── buffer.hpp      # GPU Memory & Buffer helpers
    │   ├── buffer.cpp
    │   ├── image.hpp       # Texture & Sampler helpers
    │   ├── image.cpp
    │   ├── pipeline.hpp    # Pipeline & Shader State
    │   └── pipeline.cpp
    └── renderer/           # High-level Rendering Logic
        ├── deferred_app.hpp# Main Renderer orchestration
        ├── deferred_app.cpp
        ├── gbuffer.hpp     # MRT (Multiple Render Target) management
        └── gbuffer.cpp