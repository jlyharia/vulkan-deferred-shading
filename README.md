# Defer shading

### Run/Debug Configuration (CLion)
To enable validation layers and debug markers when using a local SDK install, you must manually point the loader to the SDK binaries.

1. Go to **Run > Edit Configurations**.
2. Add the following to **Environment variables**:
   ```text
   VK_LAYER_PATH=/home/johnny/VulkanSDK/1.4.335.0/x86_64/share/vulkan/explicit_layer.d
   LD_LIBRARY_PATH=/home/johnny/VulkanSDK/1.4.335.0/x86_64/lib
   ```
3. cmake env variable
   ```text
   VULKAN_SDK=~/VulkanSDK/1.4.335.0/x86_64
   ```

## Package Management
### Conan
- conan center https://conan.io/center

Step
1. sudo apt update
2. sudo apt install pipx 
3. pipx ensurepath
4. pipx install conan
5. Install Conan plugin in clion
   ```text
   # This adds the "install" mode to your global configuration
   echo "tools.system.package_manager:mode = install" >> $(conan config home)/global.conf
   echo "tools.system.package_manager:sudo = True" >> $(conan config home)/global.conf
   ```
6. conan build command `conan install . --output-folder=build --build=missing`

## Todo
1. Delete vulkan resources per frame.
2. Move vulkan resource destroy into queue.
3. Show frame rate on screen UI.
   Summary of Priority

~~Refactor to Dynamic Rendering: Clean up the Vulkan boilerplate first.~~
ImGui Setup (High Priority): *
move to gltf
Here is the recommended order: PBR → Shadow Mapping → Deferred Shading.

Implement G-Buffer: Create the textures and the "Geometry" shaders.

Lighting Pass: Create the "Second Pass" that reads those textures.

Abstraction: Wrap these into classes like Shader, Buffer, and Texture to make the engine "Professional."

# What I have done
1. Vulkan 1.3
2. Dynamic Rendering
3. ImGui
3. Gltf