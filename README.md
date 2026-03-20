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
   PBR Shader	Hard	Adds realistic metal, roughness, and light math.
   Shadow Maps	Very Hard	Adds actual shadows cast by the arches onto the floor.
# What I have done
- Vulkan 1.3
- Dynamic Rendering 
- ImGui 
- Gltf 
- Mipmapping 
- Normal Mapping 
- add 4 spheres object to indicate point light and using ssbo to reduce attribute count in shader
- pbr without image based lighting
- horizontal grid plan
- more objects for deferred shading
- Depth reconstruction

horizontal grid plan
Here is the recommended order: PBR → Shadow Mapping → Deferred Shading.

Implement G-Buffer: Create the textures and the "Geometry" shaders.

Lighting Pass: Create the "Second Pass" that reads those textures.


七、給你一個實戰任務（面試加分）

試做兩個版本：

G-buffer 存 position

用 depth reconstruct

然後：

用 RenderDoc 看 bandwidth

比較 memory usage

測 frame time

這種「實測」會讓你在面試非常有說服力。

如果你願意，我下一步可以帶你深入一個更硬核問題：

👉 為什麼 depth 是 non-linear？
👉 或怎麼做 reversed Z + infinite far plane？
Abstraction: Wrap these into classes like Shader, Buffer, and Texture to make the engine "Professional."



# 面試導向 Vulkan / Rendering Roadmap

## Phase 0：底層打底（必做）
**目的：展示對 GPU memory / pipeline 的理解**

- **Custom GPU memory allocator**
   - Linear per-frame allocator
   - General free-list allocator
   - Optional: buddy allocator
   - 面試重點：memory lifetime、fragmentation、throughput trade-off

- **Command buffer & frame-in-flight management**
   - 支援 2~3 frames in flight
   - 每 frame 有自己的 allocator + descriptor pool
   - 理解 pipeline barrier、fence、semaphore

- **ImGui / debug UI integration**
   - 用來監控 allocator狀態、G-buffer大小、frame time
   - 面試加分：展示系統級 debugging

❌ 不用先做 shader 或 fancy post-processing

---

## Phase 1：Frame Architecture & Deferred Rendering（必做）
**目的：展示可擴展的渲染 pipeline，而不只是美圖**

- **Render graph / Frame graph**
   - 每個 pass 描述依賴資源
   - Scheduler 自動排 pass 與 barrier
   - 可動態新增 pass

- **Deferred rendering**
   - G-buffer: normal / albedo / roughness / metallic
   - Lighting pass 讀 G-buffer + shadow map
   - Depth reconstruct 取代 position buffer
   - Optional: simple SSAO（demo 不需完整實作）

- **Shadow pass**
   - 基本 directional shadow map
   - 可整合進 frame graph

❌ 不要一開始做 forward rendering + PBR 或 bloom/post effects

---

## Phase 2：高階加分（選做）
**目的：展示 pipeline cost & modern rendering understanding**

- **Simplified PBR**
   - Cook-Torrance / Fresnel-Schlick
   - IBL (environment map)
   - 不用 full dynamic GI / SSR

- **Reversed Z + infinite far plane**
   - Depth precision 優化
   - 面試口語加分

- **Attachment aliasing / memory optimization**
   - 減少 bandwidth / memory footprint
   - 展示 resource lifetime & trade-offs

- **Optional mini-engine demo**
   - Forward/deferred toggle
   - Frame graph + allocator + deferred lighting + simple PBR
   - ImGui 顯示 debug stats

❌ 不用做：
- Full cinematic PBR
- Ray tracing / path tracing
- Advanced post-processing（bloom, SSR 等）
- Fancy animation / skeletal system

---

## Phase 3：面試講法策略
- 每個 project 都能講 **設計 trade-off**：
   - Memory bandwidth vs ALU compute
   - Allocation strategy vs fragmentation
   - Frame graph vs naive call sequence
   - Depth buffer precision trade-offs (reversed Z / infinite far)

- Demo **不用炫圖**：
   - 面試官更在意 system thinking & pipeline insight

---

## 建議順序
1. 自訂 allocator + frame-in-flight → 打底
2. Render graph + deferred rendering → 核心 architecture
3. Depth reconstruct + reversed Z → optimization insight
4. Optional: 簡化 PBR → pipeline integration
5. I intend to have defer rendering + ssao and optimize it with render graph + reduce memory bandwidth and reconstruct position from depth
> 如果面試時間有限，做到第 3 步就足夠殺了。