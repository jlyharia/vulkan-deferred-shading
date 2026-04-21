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



┌──────────┬───────────────────────────────────────────┬─────────────────────────────┬───────────────────────────────────────────────────────────────────────────┐               
│ Priority │                  Feature                  │            Time             │                                    Why                                    │
├──────────┼───────────────────────────────────────────┼─────────────────────────────┼───────────────────────────────────────────────────────────────────────────┤
│ 1        │ ~~PCF (3×3)~~                                │ 30 min                      │ Removes hard-edge embarrassment from every screenshot                     │
├──────────┼───────────────────────────────────────────┼─────────────────────────────┼───────────────────────────────────────────────────────────────────────────┤
│ 2        │ Reversed-Z + infinite far plane           │ 1 day                       │ Known correctness gap, interviewers ask about depth precision             │                 
├──────────┼───────────────────────────────────────────┼─────────────────────────────┼───────────────────────────────────────────────────────────────────────────┤                 
│ 3        │ ACES / Uncharted 2 tone mapping           │ 1 hour                      │ Reinhard is visually outdated and signals you haven't kept up             │                 
├──────────┼───────────────────────────────────────────┼─────────────────────────────┼───────────────────────────────────────────────────────────────────────────┤                 
│ 4        │ Render graph                              │ 1–2 weeks                   │ Highest architecture signal, fixes the Renderer god object                │               
├──────────┼───────────────────────────────────────────┼─────────────────────────────┼───────────────────────────────────────────────────────────────────────────┤                 
│ 5        │ Profile with RenderDoc + document numbers │ 1 day                       │ Every interview will ask; having GPU timings shows engineering discipline │               
├──────────┼───────────────────────────────────────────┼─────────────────────────────┼───────────────────────────────────────────────────────────────────────────┤                 
│ 6        │ SSAO blur → compute shader                │ 2–3 days                    │ Only if time permits; plugs the "no compute" gap                          │               
├──────────┼───────────────────────────────────────────┼─────────────────────────────┼───────────────────────────────────────────────────────────────────────────┤                 
│ 7        │ CSM                                       │ Skip until after interviews │ Render graph first; CSM without it is messy to implement                  │               
└──────────┴───────────────────────────────────────────┴─────────────────────────────┴───────────────────────────────────────────────────────────────────────────┘

## Todo
- horizontal grid plan
- Percentage Closer Filtering
- Being able to explain bandwidth/ALU tradeoffs verbally — more valuable than any tool
- Reverse Z + infinite far plane — a classic optimization that shows you understand depth precision
- write a simple memory allocator
- Tiled deferred/Clustered deferred
  -  understand it deeply, implement it after you're hired or after shadows.
  - 
    ```text
     Portfolio Verdict
  
      Tiled deferred is the right next investment here. Here's why:
  
      - It's the technique interviewers ask about when they see "deferred shading" on a resume — "how would you scale this to more lights?" is the follow-up question
      - Requires a compute shader, which is a distinct signal on a portfolio (most student renderers never touch compute)
      - Implementation is 300–400 lines: depth prepass → compute tile-building pass → modify lighting.frag to read tile list
      - Demonstrates you understand the G-buffer bandwidth vs. ALU trade-off at the system level
  
      Clustered is better but 3× the implementation work. Do tiled first. If you finish tiled and want to differentiate further, clustered is the upgrade.
  
    ```
- AAA Production + Portfolio Reviewer: Stop adding features after the render graph and focus on presentation. A renderer with 8 features you can explain deeply beats one with 12    
features you're fuzzy on. Your Reinhard tone mapper is a known gap — swap it for ACES or Uncharted 2 filmic before interviews. Takes an hour, signals you know the difference.

- Engine Architect: One compute shader pass would complete the picture. You have zero compute usage right now. Interviewers at engine companies will ask. The SSAO blur is the       
natural candidate — move it from a graphics pipeline pass to a compute shader with shared-memory tile optimization. Shows you can cross the graphics/compute boundary in Vulkan,
which is table stakes at id, Frostbite-era studios, and most modern engines.

- GPU Systems: RenderDoc a frame and profile it before interviews. You should know your GPU timeline — how long each pass takes, whether you're bandwidth-bound or ALU-bound on the  
lighting pass, what the SSAO sampling cost is. If an interviewer asks "how fast is your renderer?" and you don't have numbers, that's a red flag regardless of what features you
have.


# What I have done & learn
- Vulkan 1.3
- Vulkan CPP wrapper
- Dynamic Rendering 
- Add simple UI by ImGui 
- load model by modern industrial standard Gltf 
- Mipmapping
- add 24 spheres object to indicate point light and using ssbo to reduce attribute count in shader
- pbr without image based lighting
- Defer shading
- Reconstruct position from depth buffer to reduce bandwidth from removing gbuffer position texture
- Render Doc to analyze gbuffer texture
- Nsight to analyze frame time and gpu usage
- SSAO with SSAO blur
- shadow map with one direction light
- Use renderdoc to check each shader's input and output texture
- simple directional light shadow map with jiggle edge, no PCF(Percentage Closer Filtering)
- add Percentage closer filtering 
- Sample Render graph with texture

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