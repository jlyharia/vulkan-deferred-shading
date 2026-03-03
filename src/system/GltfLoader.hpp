//
// Created by johnny on 2/24/26.
//
#pragma once

#include <string>
#include <vector>
#include <memory>
// IMPORTANT: These must match vendor_impl.cpp exactly
// so the compiler looks for the same function signatures
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>
#include "../common/Vertex.hpp"

struct GltfMesh {
    uint32_t firstIndex;
    uint32_t indexCount;
    int materialIndex;
};

class GltfLoader {
public:
    struct ModelData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<GltfMesh> meshes;
        bool success = false;
    };

    static ModelData loadFromFile(const std::string &path, bool isBinary);

private:
    static void processNode(const tinygltf::Model &input,
                            const tinygltf::Node &node,
                            ModelData &output,
                            glm::mat4 parentTransform);
    static void processPrimitive(const tinygltf::Model &input,
                                 const tinygltf::Primitive &primitive,
                                 ModelData &output,
                                 const glm::mat4 &nodeTransform);
};