//
// Created by johnny on 2/24/26.
//

#include "GltfLoader.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <iostream>

GltfLoader::ModelData GltfLoader::loadFromFile(const std::string &path) {
    tinygltf::Model input;
    tinygltf::TinyGLTF context;
    std::string warn, err;
    ModelData output;

    bool fileLoaded = false;
    if (path.substr(path.find_last_of(".") + 1) == "glb") {
        fileLoaded = context.LoadBinaryFromFile(&input, &err, &warn, path);
    } else {
        fileLoaded = context.LoadASCIIFromFile(&input, &err, &warn, path);
    }

    if (!warn.empty())
        std::cout << "glTF Warn: " << warn << std::endl;
    if (!err.empty())
        std::cerr << "glTF Err: " << err << std::endl;
    if (!fileLoaded)
        throw std::runtime_error("Failed to load glTF file: " + path);

    // 取得預設場景
    const tinygltf::Scene &scene = input.scenes[input.defaultScene > -1 ? input.defaultScene : 0];

    // 遞迴遍歷所有節點
    for (int nodeIdx : scene.nodes) {
        processNode(input, input.nodes[nodeIdx], output);
    }

    return output;
}

void GltfLoader::processNode(const tinygltf::Model &input, const tinygltf::Node &node, ModelData &output) {
    if (node.mesh > -1) {
        const tinygltf::Mesh &mesh = input.meshes[node.mesh];
        for (const auto &primitive : mesh.primitives) {
            processPrimitive(input, primitive, output);
        }
    }

    for (int childIdx : node.children) {
        processNode(input, input.nodes[childIdx], output);
    }
}

void GltfLoader::processPrimitive(const tinygltf::Model &input, const tinygltf::Primitive &primitive,
                                  ModelData &output) {
    const float *bufferPos = nullptr;
    const float *bufferNormals = nullptr;
    const float *bufferTexCoords = nullptr;
    int posStride = 0, normStride = 0, texStride = 0;
    uint32_t vertexStart = static_cast<uint32_t>(output.vertices.size());

    // --- 1. 提取屬性 (Position, Normal, UV) ---
    if (primitive.attributes.count("POSITION")) {
        const tinygltf::Accessor &acc = input.accessors[primitive.attributes.at("POSITION")];
        const tinygltf::BufferView &view = input.bufferViews[acc.bufferView];
        bufferPos = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[
            acc.byteOffset + view.byteOffset]));
        posStride = acc.ByteStride(view) / sizeof(float);
    }

    if (primitive.attributes.count("NORMAL")) {
        const tinygltf::Accessor &acc = input.accessors[primitive.attributes.at("NORMAL")];
        const tinygltf::BufferView &view = input.bufferViews[acc.bufferView];
        bufferNormals = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[
            acc.byteOffset + view.byteOffset]));
        normStride = acc.ByteStride(view) / sizeof(float);
    }

    if (primitive.attributes.count("TEXCOORD_0")) {
        const tinygltf::Accessor &acc = input.accessors[primitive.attributes.at("TEXCOORD_0")];
        const tinygltf::BufferView &view = input.bufferViews[acc.bufferView];
        bufferTexCoords = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[
            acc.byteOffset + view.byteOffset]));
        texStride = acc.ByteStride(view) / sizeof(float);
    }

    // --- 2. 組合頂點數據 ---
    size_t count = input.accessors[primitive.attributes.at("POSITION")].count;
    for (size_t v = 0; v < count; v++) {
        Vertex vert{};
        vert.pos = glm::make_vec3(&bufferPos[v * posStride]);
        vert.normal = bufferNormals ? glm::make_vec3(&bufferNormals[v * normStride]) : glm::vec3(0.0f, 1.0f, 0.0f);

        if (bufferTexCoords) {
            vert.uv = glm::make_vec2(&bufferTexCoords[v * texStride]);
            vert.uv.y = 1.0f - vert.uv.y; // Vulkan UV flip
        }
        vert.color = glm::vec3(1.0f);
        output.vertices.push_back(vert);
    }

    // --- 3. 提取索引 (Indices) ---
    const tinygltf::Accessor &indexAcc = input.accessors[primitive.indices];
    const tinygltf::BufferView &indexView = input.bufferViews[indexAcc.bufferView];
    const tinygltf::Buffer &indexBuffer = input.buffers[indexView.buffer];
    uint32_t indexCount = static_cast<uint32_t>(indexAcc.count);
    uint32_t firstIndex = static_cast<uint32_t>(output.indices.size());

    if (indexAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        const uint32_t *buf = reinterpret_cast<const uint32_t *>(&indexBuffer.data[
            indexAcc.byteOffset + indexView.byteOffset]);
        for (size_t i = 0; i < indexCount; i++)
            output.indices.push_back(buf[i] + vertexStart);
    } else if (indexAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const uint16_t *buf = reinterpret_cast<const uint16_t *>(&indexBuffer.data[
            indexAcc.byteOffset + indexView.byteOffset]);
        for (size_t i = 0; i < indexCount; i++)
            output.indices.push_back(buf[i] + vertexStart);
    }

    // 紀錄這個 Mesh 的範圍
    output.meshes.push_back({firstIndex, indexCount, primitive.material});
}