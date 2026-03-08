//
// Created by johnny on 2/24/26.
//
#pragma once

#include <common/VulkanInclude.hpp>
#include <string>
#include <vector>
#include <memory>
#include <tiny_gltf.h>
#include "../common/Vertex.hpp"
#include "scene/Model.hpp"
#include "scene/TextureManager.hpp"


struct Texture;

class GltfLoader {
public:
    struct ModelData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<Model::Submesh> primitives;
        // Maps Material ID -> Texture ID
        std::map<int, int> materialToTexture;
        std::vector<Texture> textures;
        bool success = false;
    };

    // Constructor: Needs the manager to upload textures to GPU
    explicit GltfLoader(TextureManager &textureManager) : textureManager_(textureManager) {
    }

    [[nodiscard]] ModelData loadFromFile(const std::string &path, bool isBinary) const;

private:
    TextureManager &textureManager_;

    void loadImages(const tinygltf::Model &model, ModelData &output) const;
    static void processNode(const tinygltf::Model &input,
                            const tinygltf::Node &node,
                            ModelData &output,
                            glm::mat4 parentTransform);
    static void processPrimitive(const tinygltf::Model &input,
                                 const tinygltf::Primitive &primitive,
                                 ModelData &output,
                                 const glm::mat4 &nodeTransform);
};