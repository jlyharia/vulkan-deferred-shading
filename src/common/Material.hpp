//
// Created by johnny on 3/5/26.
//

#pragma once

#include "common/VulkanInclude.hpp"

#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>

// Include your Vertex definition here or ensure it's in your project's include path
#include "Vertex.hpp"

// --- 1. GEOMETRY TYPES ---

/**
 * Represents a specific part of a larger mesh.
 * In Sponza, one mesh (like 'Lion') might be split into multiple submeshes
 * because different parts use different materials.
 */
struct Submesh {
    uint32_t firstIndex;
    uint32_t indexCount;
    int materialIndex; // Index into the Model's material vector
};

// --- 2. LOADER TYPES ---

/**
 * A lightweight info struct used by the GltfLoader to communicate
 * material requirements back to the Model class.
 */
struct MaterialInfo {
    std::string name;

    // Indices into the ModelData::textures vector
    int baseColorIdx = -1;
    int normalIdx = -1;
    int metallicRoughnessIdx = -1;

    // Material constants
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;

    // Pipeline states
    bool doubleSided = false;
    float alphaCutoff = 0.5f;
    bool isTransparent = false; // Determined by glTF 'alphaMode'
};

/**
 * Model own the texture resource
 */
struct ModelData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> primitives;

    // Maps glTF Material Index -> Our MaterialInfo
    std::map<int, MaterialInfo> materials;

    // We assume Texture is your struct containing vk::Image, VmaAllocation, etc.
    std::vector<Texture> textures;

    bool success = false;
};

// --- 3. THE MATERIAL CLASS ---

/**
 * The Material class manages the Vulkan DescriptorSet (Set 1)
 * and the specific PBR parameters for a surface.
 */
class Material {
public:
    std::string name;

    /** * The baked Descriptor Set for Set 1.
     * Binding 0: sampler2D baseColor
     * Binding 1: sampler2D normalMap
     * Binding 2: sampler2D metallicRoughness (Optional but recommended)
     */
    vk::DescriptorSet textureSet;

    // --- PBR Parameters ---
    // These should be sent to the shader via Push Constants
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;

    // --- Pipeline State ---
    bool doubleSided = false; // Disables Backface Culling
    float alphaCutoff = 0.5f; // Alpha testing threshold
    bool isTransparent = false; // Enables Alpha Blending

    // --- Resource Management ---
    // Storing these allows the Model to track which textures are in use
    int baseColorIndex = -1;
    int normalMapIndex = -1;
    int metallicRoughnessIndex = -1;

    // Constructor/Destructor can be added here if you want to manage
    // the descriptor set lifetime directly.
};


