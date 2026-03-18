//
// Created by johnny on 3/5/26.
//

#pragma once

#include "common/VulkanInclude.hpp"
#include "scene/Texture.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>

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
    int materialIndex; // Index into the Mesh's material vector
};

// --- 2. LOADER TYPES ---

/**
 * A lightweight info struct used by the GltfLoader to communicate
 * material requirements back to the AssetManager.
 */
struct MaterialInfo {
    std::string name;

    // Indices into the MeshData::textures vector
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
    bool isTransparent = false;
};

/**
 * Intermediate data returned by GltfLoader to AssetManager.
 * Textures are shared_ptr so AssetManager can move them directly into Materials.
 */
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> primitives;

    // Maps glTF Material Index -> Our MaterialInfo
    std::map<int, MaterialInfo> materials;

    // shared_ptr: TextureManager owns the GPU resource, Materials share ownership
    std::vector<std::shared_ptr<Texture>> textures;

    bool success = false;
};

// --- 3. THE MATERIAL CLASS ---

/**
 * Holds PBR surface parameters and shared ownership of its textures.
 * Texture lifetime: Material alive => shared_ptr alive => GPU resource alive.
 */
class Material {
public:
    std::string name;

    // Texture ownership — shared with TextureManager's cache
    std::shared_ptr<Texture> albedoTexture;
    std::shared_ptr<Texture> normalTexture;
    std::shared_ptr<Texture> metallicRoughnessTexture;

    /**
     * The baked Descriptor Set for Set 1.
     * Binding 0: sampler2D baseColor
     * Binding 1: sampler2D normalMap
     * Binding 2: sampler2D metallicRoughness
     */
    vk::DescriptorSet textureSet;

    // --- PBR Parameters ---
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;

    // --- Pipeline State ---
    bool doubleSided = false;
    float alphaCutoff = 0.5f;
    bool isTransparent = false;
    bool unlit = false; // skips lighting — use for emissive objects like light indicators
};
