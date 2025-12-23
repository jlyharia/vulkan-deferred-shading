# Defer shading

### Run/Debug Configuration (CLion)
To enable validation layers and debug markers when using a local SDK install, you must manually point the loader to the SDK binaries.

1. Go to **Run > Edit Configurations**.
2. Add the following to **Environment variables**:
   ```text
   VK_LAYER_PATH=/home/johnny/VulkanSDK/1.4.335.0/x86_64/share/vulkan/explicit_layer.d
   LD_LIBRARY_PATH=/home/johnny/VulkanSDK/1.4.335.0/x86_64/lib