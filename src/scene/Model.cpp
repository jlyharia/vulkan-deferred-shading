//
// Created by johnny on 2/25/26.
//

#include "Model.hpp"
#include "system/GltfLoader.hpp"
#include <tiny_obj_loader.h>
#include <tiny_gltf.h>
#include <unordered_map>
#include "../vulkan/VulkanUtils.hpp"
#include "renderer/renderer.hpp"

#include <fstream>
#include <iostream>
#include <algorithm> // Required for std::ranges::transform
#include <cctype>    // Required for ::tolower
#include <filesystem>

Model::~Model() {
    // VMA is safe: it ignores null handles, but checking is cleaner
    if (vertexBuffer_.buffer) {
        vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(vertexBuffer_.buffer), vertexBuffer_.allocation);
        vertexBuffer_.buffer = nullptr; // Optional but clean
        vertexBuffer_.allocation = nullptr;
    }

    if (indexBuffer_.buffer) {
        vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(indexBuffer_.buffer), indexBuffer_.allocation);
        indexBuffer_.buffer = nullptr;
        indexBuffer_.allocation = nullptr;
    }
}

bool Model::loadFromFile(const std::string &filePath, TextureManager &textureManager, Renderer &renderer) {
    namespace fs = std::filesystem;
    fs::path path(filePath);

    // 1. Check if file exists
    if (!fs::exists(path)) {
        std::cerr << "[Model] File does not exist: " << filePath << std::endl;
        return false;
    }

    // 2. Extract extension and normalize to lowercase
    std::string ext = path.extension().string();
    // std::ranges::transform(ext, ext.begin(), ::tolower);
    // 2. Modern C++20 Lowercase Transformation
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    // 3. Handle OBJ directly
    // if (ext == ".obj") {
    //     return loadObj(filePath);
    // }

    // 4. Handle glTF / glb using Header Detection (Magic Number)
    if (ext == ".gltf" || ext == ".glb") {
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
            return false;

        char header[4];
        file.read(header, 4);
        file.close();

        // "glTF" in ASCII is the magic number for binary .glb
        bool isBinary = (std::strncmp(header, "glTF", 4) == 0);

        return loadGltf(filePath, isBinary, textureManager, renderer);
    }

    std::cerr << "[Model] Unsupported file extension: " << ext << std::endl;
    return false;
}


/**
 *
2. The "Handle" Pattern (Industrial Standard)

In industrial engines, the Model doesn't know how to render; it just holds Handles.
Think of the vk::DescriptorSet as a "key." The Model holds the key, but only the Renderer knows how to use that key to open the "GPU door."

If you don't pass the Renderer (or a resource factory) into loadGltf, your Model would just have a list of image file paths. Then, every frame, the Renderer would have to check: "Do I have a descriptor for this path yet?" This is extremely slow.
3. Better Architecture: The "Resource Factory"

If passing the entire Renderer into your Model feels too "heavy" (and I agree, it is), the professional way to fix this is to use a Resource Factory or Descriptor Allocator.

Instead of:
bool loadGltf(..., Renderer &renderer)

todo Use:
bool loadGltf(..., IDescriptorAllocator &allocator)
 */
// bool Model::loadGltf(const std::string &filePath,
//                      const bool isBinary,
//                      TextureManager &textureManager,
//                      Renderer &renderer) {
//     // We pass the isBinary flag we detected in the header check
//
//     const GltfLoader gltf_loader(textureManager);
//
//     auto data = gltf_loader.loadFromFile(filePath, isBinary);
//
//     if (!data.success) {
//         return false;
//     }
//
//     // Clear old data for re-loading safety
//     vertices_.clear();
//     indices_.clear();
//     submeshes_.clear();
//
//     // 1. Move basic geometry
//     // Move data (Zero-copy)
//     vertices_ = std::move(data.vertices);
//     indices_ = std::move(data.indices);
//     indexCount_ = static_cast<uint32_t>(indices_.size());
//
//     for (const auto &m : data.primitives) {
//         submeshes_.push_back({m.firstIndex, m.indexCount, m.materialIndex});
//     }
//
//     // 2. Create Materials and Descriptor Sets
//     this->materials_.reserve(data.textures.size());
//     for (const auto &tex : data.textures) {
//         Material mat;
//         mat.name = "Sponza_Material";
//
//         // THE KEY STEP: Ask the renderer to create a Set 1 for this texture
//         // This function would allocate from the pool and write the descriptor
//         mat.textureSet = renderer.createTextureDescriptorSet(tex.imageView, tex.sampler);
//
//         this->materials_.push_back(mat);
//     }
//     std::cout << "[Model] Loaded glTF: " << filePath << " (" << vertices_.size() << " verts)" << std::endl;
//     return true;
// }

bool Model::loadGltf(const std::string &filePath,
                     const bool isBinary,
                     TextureManager &textureManager,
                     Renderer &renderer) {

    GltfLoader gltf_loader(textureManager);
    auto data = gltf_loader.loadFromFile(filePath, isBinary);

    if (!data.success)
        return false;

    // 1. Geometry Move
    vertices_ = std::move(data.vertices);
    indices_ = std::move(data.indices);
    submeshes_ = std::move(data.primitives);

    // 2. Create Descriptor Sets for every image loaded in the glTF
    // We do this first so we have a 'pool' of textures to pick from
    std::vector<vk::DescriptorSet> textureSets;
    textureSets.reserve(data.textures.size());
    for (const auto &tex : data.textures) {
        textureSets.push_back(renderer.createTextureDescriptorSet(tex.imageView, tex.sampler));
    }

    // 3. Match Materials to those Sets
    // We need to size this according to how many materials the loader found
    this->materials_.clear();

    // Determine the number of materials in the glTF
    // If the map is empty, we have no materials to process
    if (!data.materialToTexture.empty()) {
        int maxMatIdx = 0;
        for (auto const &[idx, _] : data.materialToTexture) {
            maxMatIdx = std::max(maxMatIdx, idx);
        }
        materials_.resize(maxMatIdx + 1);

        for (int i = 0; i <= maxMatIdx; ++i) {
            if (data.materialToTexture.contains(i)) {
                int imgIdx = data.materialToTexture.at(i);

                // If this material points to a valid image, link the DescriptorSet
                if (imgIdx != -1 && imgIdx < textureSets.size()) {
                    materials_[i].textureSet = textureSets[imgIdx];
                }
            }
        }
    }

    std::cout << "[Model] Finalized " << materials_.size() << " materials for " << filePath << std::endl;
    return true;
}

void Model::uploadToGPU(vk::Device device, vk::Queue queue, vk::CommandPool commandPool) {

    if (vertices_.empty() || indices_.empty()) {
        std::cerr << "[Model Error] Attempted to upload empty mesh data to GPU! Check loader." << std::endl;
        return;
    }
    // 1. Vertex Buffer Upload
    // Note: vertexBuffer_ is assumed to be an 'AllocatedBuffer' struct
    vk_util::uploadToDeviceBuffer(
        allocator_,
        device,
        queue,
        commandPool,
        vertices_,
        vk::BufferUsageFlagBits::eVertexBuffer,
        vertexBuffer_.buffer,
        vertexBuffer_.allocation
        );

    // 2. Index Buffer Upload
    vk_util::uploadToDeviceBuffer(
        allocator_,
        device,
        queue,
        commandPool,
        indices_,

        vk::BufferUsageFlagBits::eIndexBuffer,
        indexBuffer_.buffer,
        indexBuffer_.allocation
        );

    // --- NEW: Discard CPU data ---
    // We clear the vectors because this is a rigid body.
    // We don't need to perform CPU-side skinning or deformation.
    vertices_.clear();
    vertices_.shrink_to_fit();

    indices_.clear();
    indices_.shrink_to_fit();

    std::cout << "[Model] GPU upload complete. CPU memory cleared." << std::endl;
}