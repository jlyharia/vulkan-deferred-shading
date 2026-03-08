//
// Created by johnny on 2/24/26.
//

#include "GltfLoader.hpp"

#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

GltfLoader::ModelData GltfLoader::loadFromFile(const std::string &path, const bool isBinary) const {
    tinygltf::Model input;
    tinygltf::TinyGLTF context;
    std::string warn, err;
    ModelData modelData;
    modelData.success = false;

    // 1. Path processPrimitive
    // This ensures we don't even try to load if the file is missing from your symlinked assets
    if (!std::filesystem::exists(path)) {
        std::cerr << "[glTF Err] File does not exist at: " << path << std::endl;
        std::cerr << "[glTF Err] Check if your 'assets' symlink in cmake-build-debug is broken." << std::endl;
        return modelData;
    }
    std::cout << "[Debug] Attempting to load glTF from: "
        << std::filesystem::absolute(path) << std::endl;
    std::cout << "[Debug] Looking for .bin in: "
        << std::filesystem::absolute(path).parent_path() << std::endl;

    // 2. Disable internal image loading
    // Since you don't have the stb_image callback set up yet, this prevents a crash
    // context.SetImageLoader(nullptr, nullptr);
    // context.SetImageLoader([](tinygltf::Image *, const int, std::string *,
    //                           std::string *, int, int, const unsigned char *,
    //                           int, void *) {
    // return true;
    // }, nullptr);
    // 2. RE-ENABLE Image Loading
    // We use tinygltf's default loader (stb_image) to get the raw pixels
    // but we won't upload them to Vulkan yet.
    const bool fileLoaded = isBinary
                                ? context.LoadBinaryFromFile(&input, &err, &warn, path)
                                : context.LoadASCIIFromFile(&input, &err, &warn, path);

    // 3. LOAD TEXTURES FIRST
    // This populates output.textures via your TextureManager
    loadImages(input, modelData);

    if (!warn.empty())
        std::cout << "[glTF Warn]: " << warn << std::endl;
    if (!err.empty())
        std::cerr << "[glTF Err]: " << err << std::endl;

    if (!fileLoaded) {
        std::cerr << "[glTF Err] Failed to parse glTF: " << path << std::endl;
        return modelData;
    }

    modelData.materialToTexture.clear();
    for (size_t i = 0; i < input.materials.size(); i++) {
        const auto &gltfMat = input.materials[i];

        // We specifically want the Base Color (Diffuse) map
        const int baseColorTexIndex = gltfMat.pbrMetallicRoughness.baseColorTexture.index;

        if (baseColorTexIndex != -1) {
            // glTF Texture index -> glTF Image source index
            const int imageIndex = input.textures[baseColorTexIndex].source;
            modelData.materialToTexture[static_cast<int>(i)] = imageIndex;
        } else {
            // No texture for this material (use -1 to trigger white fallback later)
            modelData.materialToTexture[static_cast<int>(i)] = -1;
        }
    }
    // 4. SCENE TRAVERSAL
    // Sponza often has many nodes; we must traverse starting from the default scene
    const tinygltf::Scene &scene = input.scenes[input.defaultScene > -1 ? input.defaultScene : 0];

    // Optimization: Pre-allocate memory to handle Sponza's high vertex count
    modelData.vertices.reserve(200000);
    modelData.indices.reserve(200000);

    for (int nodeIdx : scene.nodes) {
        processNode(input, input.nodes[nodeIdx], modelData, glm::mat4(1.0f));
    }

    // 6. Success Check
    if (modelData.vertices.empty()) {
        std::cerr << "[glTF Err] Logic error: Loaded file but found 0 vertices. Check processNode recursion." <<
            std::endl;
    } else {
        modelData.success = true;
        std::cout << "[glTF Success] " << path << ":" << std::endl;
        std::cout << "   - Vertices: " << modelData.vertices.size() << std::endl;
        std::cout << "   - Primitives: " << modelData.primitives.size() << std::endl;
        std::cout << "   - Textures: " << modelData.textures.size() << std::endl;
    }

    return modelData;
}

// Update your function signature to accept a parent transform
void GltfLoader::processNode(const tinygltf::Model &input,
                             const tinygltf::Node &node,
                             ModelData &output,
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
                                  ModelData &output,
                                  const glm::mat4 &nodeTransform) {

    const float *bufferPos = nullptr;
    const float *bufferNormals = nullptr;
    const float *bufferTexCoords = nullptr;
    int posStride = 0, normStride = 0, texStride = 0;

    uint32_t vertexStart = static_cast<uint32_t>(output.vertices.size());

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
    }

    if (primitive.attributes.contains("TEXCOORD_0")) {
        const tinygltf::Accessor &acc = input.accessors[primitive.attributes.at("TEXCOORD_0")];
        const tinygltf::BufferView &view = input.bufferViews[acc.bufferView];
        bufferTexCoords = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[
            acc.byteOffset + view.byteOffset]));
        texStride = acc.ByteStride(view) / sizeof(float);
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

void GltfLoader::loadImages(const tinygltf::Model &model, ModelData &output) const {
    std::cout << "[glTF] Uploading " << model.images.size() << " textures to GPU..." << std::endl;
    output.textures.resize(model.images.size());
    for (size_t i = 0; i < model.images.size(); i++) {
        const auto &gltfImage = model.images[i];

        // 2. Load and place specifically at index 'i'
        // This ensures output.textures[i] corresponds to tinygltf image index 'i'
        output.textures[i] = textureManager_.loadTextureFromGltf(gltfImage);

        // Optional: Debugging log to track progress
        std::cout << "  [Texture " << i << "] " << gltfImage.name << " loaded." << std::endl;
    }
}