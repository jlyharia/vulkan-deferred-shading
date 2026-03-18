//
// Created by johnny on 2/24/26.
//

#include "GltfLoader.hpp"

#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

MeshData GltfLoader::loadFromFile(const std::string &path, const bool isBinary) const {
    tinygltf::Model input;
    tinygltf::TinyGLTF context;
    std::string warn, err;
    MeshData modelData;
    modelData.success = false;

    if (!std::filesystem::exists(path)) {
        std::cerr << "[glTF Err] Path invalid: " << path << std::endl;
        return modelData;
    }

    // 1. Load raw file data
    const bool fileLoaded = isBinary
                                ? context.LoadBinaryFromFile(&input, &err, &warn, path)
                                : context.LoadASCIIFromFile(&input, &err, &warn, path);

    if (!warn.empty())
        std::cout << "[glTF Warn]: " << warn << std::endl;
    if (!err.empty())
        std::cerr << "[glTF Err]: " << err << std::endl;
    if (!fileLoaded)
        return modelData;

    // 2. Process Textures (Delegated to helper)
    loadImages(input, modelData);

    // 3. Process Materials (PBR Refactor)
    modelData.materials.clear();
    for (size_t i = 0; i < input.materials.size(); i++) {
        const auto &gltfMat = input.materials[i];
        MaterialInfo info;
        info.name = gltfMat.name;

        // Map glTF indices to our uploaded texture indices
        if (gltfMat.pbrMetallicRoughness.baseColorTexture.index != -1)
            info.baseColorIdx = input.textures[gltfMat.pbrMetallicRoughness.baseColorTexture.index].source;

        if (gltfMat.normalTexture.index != -1)
            info.normalIdx = input.textures[gltfMat.normalTexture.index].source;

        if (gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
            info.metallicRoughnessIdx = input.textures[gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index].source;

        // Capture PBR factors and alpha state
        auto &pbr = gltfMat.pbrMetallicRoughness;
        info.baseColorFactor = glm::make_vec4(pbr.baseColorFactor.data());
        info.metallicFactor = static_cast<float>(pbr.metallicFactor);
        info.roughnessFactor = static_cast<float>(pbr.roughnessFactor);
        info.alphaCutoff = static_cast<float>(gltfMat.alphaCutoff);
        info.doubleSided = gltfMat.doubleSided;
        info.isTransparent = (gltfMat.alphaMode == "BLEND");

        modelData.materials[static_cast<int>(i)] = info;
    }

    // 4. Geometry Traversal
    const tinygltf::Scene &scene = input.scenes[input.defaultScene > -1 ? input.defaultScene : 0];
    modelData.vertices.reserve(200000);
    modelData.indices.reserve(200000);

    for (int nodeIdx : scene.nodes) {
        processNode(input, input.nodes[nodeIdx], modelData, glm::mat4(1.0f));
    }

    modelData.success = !modelData.vertices.empty();
    return modelData;
}

// Update your function signature to accept a parent transform
void GltfLoader::processNode(const tinygltf::Model &input,
                             const tinygltf::Node &node,
                             MeshData &output,
                             glm::mat4 parentTransform) {

    // 1. Calculate this node's world transform matrix
    // We start with the parent's transform (identity if root)
    glm::mat4 nodeTransform = parentTransform;

    if (node.matrix.size() == 16) {
        // Use the provided 4x4 matrix (Cast from double to float)
        glm::dmat4 localMatrix = glm::make_mat4(node.matrix.data());
        nodeTransform *= glm::mat4(localMatrix);
    } else {
        // Fallback: Manually build matrix from Translation, Rotation, and Scale
        if (node.translation.size() == 3) {
            glm::dvec3 t = glm::make_vec3(node.translation.data());
            nodeTransform = glm::translate(nodeTransform, glm::vec3(t));
        }
        if (node.rotation.size() == 4) {
            // Quaternions in glTF are (x, y, z, w)
            glm::dquat dq = glm::make_quat(node.rotation.data());
            nodeTransform *= glm::mat4_cast(glm::quat(dq));
        }
        if (node.scale.size() == 3) {
            glm::dvec3 s = glm::make_vec3(node.scale.data());
            nodeTransform = glm::scale(nodeTransform, glm::vec3(s));
        }
    }

    // 2. If the node has a mesh, process all its primitives
    if (node.mesh > -1) {
        // Debug check to see what is being found
        // std::cout << "[glTF] Processing mesh: " << input.meshes[node.mesh].name << std::endl;

        const tinygltf::Mesh &mesh = input.meshes[node.mesh];
        for (const auto &primitive : mesh.primitives) {
            // IMPORTANT: We pass the calculated nodeTransform to the vertex processor
            processPrimitive(input, primitive, output, nodeTransform);
        }
    }

    // 3. Recurse into children, passing this node's world transform as the new parent
    for (int childIdx : node.children) {
        processNode(input, input.nodes[childIdx], output, nodeTransform);
    }
}

void GltfLoader::processPrimitive(const tinygltf::Model &input,
                                  const tinygltf::Primitive &primitive,
                                  MeshData &output,
                                  const glm::mat4 &nodeTransform) {

    const float *bufferPos = nullptr;
    const float *bufferNormals = nullptr;
    const float *bufferTexCoords = nullptr;
    const float *bufferTangents = nullptr;
    int posStride = 0, normStride = 0, texStride = 0;

    uint32_t vertexStart = static_cast<uint32_t>(output.vertices.size());

    for (auto &[name, index] : primitive.attributes) {
        std::cout << "[Attr] " << name << std::endl;
    }
    // --- 1. Extract Attributes ---
    if (primitive.attributes.contains("POSITION")) {
        const tinygltf::Accessor &acc = input.accessors[primitive.attributes.at("POSITION")];
        const tinygltf::BufferView &view = input.bufferViews[acc.bufferView];
        bufferPos = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[
            acc.byteOffset + view.byteOffset]));
        posStride = acc.ByteStride(view) / sizeof(float);
    }

    if (primitive.attributes.contains("NORMAL")) {
        const tinygltf::Accessor &acc = input.accessors[primitive.attributes.at("NORMAL")];
        const tinygltf::BufferView &view = input.bufferViews[acc.bufferView];
        bufferNormals = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[
            acc.byteOffset + view.byteOffset]));
        normStride = acc.ByteStride(view) / sizeof(float);
    } else {
        std::cerr << "[GltfLoader::Missing Normal]" << std::endl;
    }

    if (primitive.attributes.contains("TEXCOORD_0")) {
        const tinygltf::Accessor &acc = input.accessors[primitive.attributes.at("TEXCOORD_0")];
        const tinygltf::BufferView &view = input.bufferViews[acc.bufferView];
        bufferTexCoords = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[
            acc.byteOffset + view.byteOffset]));
        texStride = acc.ByteStride(view) / sizeof(float);
    }

    if (primitive.attributes.contains("TANGENT")) {
        const tinygltf::Accessor &acc = input.accessors[primitive.attributes.at("TANGENT")];
        const tinygltf::BufferView &view = input.bufferViews[acc.bufferView];
        bufferTangents = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[
            acc.byteOffset + view.byteOffset]));
        // Note: Tangent stride is usually 4 floats (vec4)
    }

    // --- NEW: Material Factor Capture ---
    glm::vec3 baseColorFactor(1.0f); // Default to white
    if (primitive.material > -1) {
        const auto &gltfMat = input.materials[primitive.material];
        const auto &pbr = gltfMat.pbrMetallicRoughness;

        baseColorFactor = glm::vec3(
            static_cast<float>(pbr.baseColorFactor[0]),
            static_cast<float>(pbr.baseColorFactor[1]),
            static_cast<float>(pbr.baseColorFactor[2])
            );

        // DEBUG: Uncomment this to verify the arches are actually being assigned a color
        if (baseColorFactor.r < 1.0f) {
            std::cout << "[GltfLoader] Material " << primitive.material << " Factor: "
                << baseColorFactor.r << ", " << baseColorFactor.g << std::endl;
        }
    }

    // --- 2. Calculate Normal Matrix ---
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(nodeTransform)));

    // --- 3. Assemble Vertex Data ---
    size_t count = input.accessors[primitive.attributes.at("POSITION")].count;
    for (size_t v = 0; v < count; v++) {
        Vertex vert{};

        // Position
        glm::vec3 rawPos = glm::make_vec3(&bufferPos[v * posStride]);
        vert.pos = glm::vec3(nodeTransform * glm::vec4(rawPos, 1.0f));

        // Normal (Location 1)
        if (bufferNormals) {
            glm::vec3 rawNormal = glm::make_vec3(&bufferNormals[v * normStride]);
            vert.normal = glm::normalize(normalMatrix * rawNormal);
        } else {
            vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        // UV (Location 2)
        if (bufferTexCoords) {
            vert.uv = glm::make_vec2(&bufferTexCoords[v * texStride]);
            vert.uv.y = 1.0f - vert.uv.y;
        }

        // Color (Location 3)
        // This MUST be set to the baseColorFactor for the arches to show stone color
        vert.color = baseColorFactor;
        if (primitive.material == 5) // or whichever arch material
        {
            std::cout << "UV: " << vert.uv.x << ", " << vert.uv.y << std::endl;
        }

        if (bufferTangents) {
            vert.tangent = glm::make_vec4(&bufferTangents[v * 4]);
            // Most glTF tangents are vec4; use the normalMatrix to rotate them
            vert.tangent = glm::vec4(normalMatrix * glm::vec3(vert.tangent), vert.tangent.w);
        }

        output.vertices.push_back(vert);
    }

    // --- 4. Extract Indices ---
    if (primitive.indices > -1) {
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

        output.primitives.push_back({firstIndex, indexCount, primitive.material});
    }
}

void GltfLoader::loadImages(const tinygltf::Model &model, MeshData &output) const {
    if (model.images.empty())
        return;

    std::cout << "[glTF] Scanning " << model.images.size() << " textures..." << std::endl;

    // 1. Identify Data Maps (Normals, Metallic, Roughness)
    std::vector<bool> isDataMap(model.images.size(), false);
    for (const auto &mat : model.materials) {
        // Check Normal Map slot
        if (mat.normalTexture.index != -1) {
            int imageIdx = model.textures[mat.normalTexture.index].source;
            if (imageIdx != -1)
                isDataMap[imageIdx] = true;
        }
        // Check Metallic/Roughness slot
        if (mat.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
            int imageIdx = model.textures[mat.pbrMetallicRoughness.metallicRoughnessTexture.index].source;
            if (imageIdx != -1)
                isDataMap[imageIdx] = true;
        }
    }

    // 2. Upload to GPU — store shared_ptr directly from TextureManager
    output.textures.resize(model.images.size());
    for (size_t i = 0; i < model.images.size(); i++) {
        const auto &img = model.images[i];
        const bool isColor = !isDataMap[i];
        const vk::Format format = isColor ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
        const std::string key = img.uri.empty() ? img.name : img.uri;

        output.textures[i] = textureManager_.getOrCreateTexture(
            key,
            img.image.data(),
            static_cast<uint32_t>(img.width),
            static_cast<uint32_t>(img.height),
            format);

        std::cout << "  [Tex " << i << "] " << (isColor ? "sRGB" : "Unorm")
            << " | " << img.name << std::endl;
    }
}