//
// Created by johnny on 2/25/26.
//

#include "Model.hpp"
#include "system/GltfLoader.hpp"
#include <tiny_gltf.h>
#include "../vulkan/VulkanUtils.hpp"
#include "renderer/renderer.hpp"

#include <fstream>
#include <iostream>
#include <algorithm> // Required for std::ranges::transform
#include <cctype>    // Required for ::tolower
#include <filesystem>

Model::~Model() {
    // 1. Destroy Texture Resources
    for (auto &tex : textures_) {
        // MUST destroy View before Image
        if (tex.imageView) {
            device_.destroyImageView(tex.imageView);
            tex.imageView = nullptr;
        }

        if (tex.image) {
            vmaDestroyImage(allocator_, static_cast<VkImage>(tex.image), tex.allocation);
            tex.image = nullptr;
            tex.allocation = nullptr;
        }
    }

    // 2. Destroy Mesh Buffers
    if (vertexBuffer_.buffer) {
        vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(vertexBuffer_.buffer), vertexBuffer_.allocation);
    }
    if (indexBuffer_.buffer) {
        vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(indexBuffer_.buffer), indexBuffer_.allocation);
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

bool Model::loadGltf(const std::string &filePath,
                     const bool isBinary,
                     TextureManager &textureManager,
                     Renderer &renderer) {

    GltfLoader gltf_loader(textureManager);
    auto data = gltf_loader.loadFromFile(filePath, isBinary);

    if (!data.success)
        return false;

    // 1. Ownership Transfer
    vertices_ = std::move(data.vertices);
    indices_ = std::move(data.indices);
    submeshes_ = std::move(data.primitives);

    // CRITICAL: Set this before clearing indices_ in uploadToGPU
    indexCount_ = static_cast<uint32_t>(indices_.size());

    // Ownership: Move the textures into the Model so they stay alive
    this->textures_ = std::move(data.textures);

    // 2. Material Processing
    this->materials_.clear();

    for (auto const &[matIdx, info] : data.materials) {
        Material material;
        material.name = info.name;
        material.doubleSided = info.doubleSided;
        material.alphaCutoff = info.alphaCutoff;
        material.baseColorFactor = info.baseColorFactor; // Pass color to shader

        // --- RESOLVE VIEWS FROM THE NEW OWNER (this->textures_) ---
        // We use 'this->textures_' because 'data.textures' was moved and is now empty.
        const vk::ImageView colorView = (info.baseColorIdx != -1)
                                            ? this->textures_[info.baseColorIdx].imageView
                                            : textureManager.getWhiteFallback().imageView;

        const vk::ImageView normalView = (info.normalIdx != -1)
                                             ? this->textures_[info.normalIdx].imageView
                                             : textureManager.getFlatNormalFallback().imageView;

        const vk::ImageView metalRoughView = (info.metallicRoughnessIdx != -1)
                                                 ? this->textures_[info.metallicRoughnessIdx].imageView
                                                 : textureManager.getBlackFallback().imageView;

        // --- BAKE DESCRIPTOR SET ---
        // Set 1: Binding 0=Albedo, 1=Normal, 2=MetalRough
        material.textureSet = renderer.createTextureDescriptorSet(
            colorView,
            normalView,
            metalRoughView,
            textureManager.getDefaultSampler()
            );

        // Ensure the materials vector is large enough for the glTF index
        if (matIdx >= materials_.size()) {
            materials_.resize(matIdx + 1);
        }
        materials_[matIdx] = material;
    }

    std::cout << "[Model] Finalized " << materials_.size() << " PBR materials for " << filePath << std::endl;
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