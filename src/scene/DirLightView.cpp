//
// Created by johnny on 5/11/26.
//

#include "DirLightView.hpp"
#include <cmath>


#include <glm/gtx/norm.hpp> // Required for glm::length2

namespace {
// Localized internal helper methods

std::array<float, engineConfig::NUM_CASCADES + 1> calculateSplitDistances(
    float cameraNear, float shadowFar, float lambda) {
    std::array<float, engineConfig::NUM_CASCADES + 1> splits;
    splits[0] = cameraNear;
    splits[engineConfig::NUM_CASCADES] = shadowFar;

    for (int i = 1; i < engineConfig::NUM_CASCADES; i++) {
        float t = static_cast<float>(i) / static_cast<float>(engineConfig::NUM_CASCADES);
        float logSplit = cameraNear * std::pow(shadowFar / cameraNear, t);
        float uniformSplit = cameraNear + t * (shadowFar - cameraNear);
        splits[i] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
    }
    return splits;
}

std::pair<glm::vec3, float> computeBoundingSphere(
    float splitNear, float splitFar,
    float tanHalfFovX, float tanHalfFovY,
    const glm::mat4 &invCamView) {
    // Compute sub-frustum corners analytically in view space
    float hN = splitNear * tanHalfFovY, wN = splitNear * tanHalfFovX;
    float hF = splitFar * tanHalfFovY, wF = splitFar * tanHalfFovX;

    std::array<glm::vec4, 8> viewCorners = {{
        {-wN, -hN, -splitNear, 1.f}, {wN, -hN, -splitNear, 1.f},
        {-wN, hN, -splitNear, 1.f}, {wN, hN, -splitNear, 1.f},
        {-wF, -hF, -splitFar, 1.f}, {wF, -hF, -splitFar, 1.f},
        {-wF, hF, -splitFar, 1.f}, {wF, hF, -splitFar, 1.f},
    }};

    glm::vec3 sphereCenter(0.0f);
    std::array<glm::vec3, 8> worldCorners;
    for (int j = 0; j < 8; j++) {
        worldCorners[j] = glm::vec3(invCamView * viewCorners[j]);
        sphereCenter += worldCorners[j];
    }
    sphereCenter /= 8.0f;

    float radius = 0.0f;
    for (const auto &wc : worldCorners) {
        radius = std::max(radius, glm::length(wc - sphereCenter));
    }

    // Quantize radius to prevent precision scaling ripples
    radius = std::ceil(radius * 16.0f) / 16.0f;

    return {sphereCenter, radius};
}

glm::mat4 computeCascadeMatrix(
    const glm::vec3 &sphereCenter, float radius,
    const glm::vec3 &lightDir, const glm::vec3 &lightRight, const glm::vec3 &lightUp) {
    // Project the world space sphere center along our locked orientation axes
    // center x and y is the sphereCenter projection alone light right and light up
    float centerX = glm::dot(sphereCenter, lightRight);
    float centerY = glm::dot(sphereCenter, lightUp);

    // Execute Texel Snapping
    float texelSize = (2.0f * radius) / static_cast<float>(engineConfig::SHADOW_MAP_SIZE);
    centerX = std::floor(centerX / texelSize) * texelSize;
    centerY = std::floor(centerY / texelSize) * texelSize;

    // Reconstruct the snapped center location back into world space
    float centerZ = glm::dot(sphereCenter, lightDir);
    glm::vec3 snappedWorldCenter = centerX * lightRight + centerY * lightUp + centerZ * lightDir;

    // Build the look-at frame focused on the snapped world coordinates
    glm::vec3 cascadeLightPos = snappedWorldCenter - lightDir * radius;
    glm::mat4 cascadeLightView = glm::lookAt(cascadeLightPos, snappedWorldCenter, lightUp);

    // Stable symmetric orthographic bounds
    float minX = -radius;
    float maxX = radius;
    float minY = -radius;
    float maxY = radius;

    // Static depth padding completely isolated from camera motion
    // DEPTH BOUNDS (Z)
    float zNear = -radius - 100.0f;
    float zFar = radius + 100.0f;

    glm::mat4 proj = glm::orthoRH_ZO(minX, maxX, minY, maxY, zNear, zFar);
    proj[1][1] *= -1.0f; // Vulkan NDC Y-flip

    // Reverse-Z mapping: remap depth [0, 1] -> [1, 0]
    glm::mat4 revZ(1.0f);
    revZ[2][2] = -1.0f;
    revZ[3][2] = 1.0f;

    return revZ * proj * cascadeLightView;
}

} // namespace

std::array<CascadeData, engineConfig::NUM_CASCADES>
DirLightView::computeCascades(const glm::mat4 &cameraView, const glm::mat4 &cameraProj, float lambda) const {

    float cameraNear = cameraProj[3][2];
    float tanHalfFovY = std::abs(1.0f / cameraProj[1][1]);
    float tanHalfFovX = 1.0f / cameraProj[0][0];

    auto splits = calculateSplitDistances(cameraNear, shadowFar, lambda);

    glm::vec3 lightDir = glm::normalize(target - position);
    glm::mat4 invCamView = glm::inverse(cameraView);

    // Establish a rigid coordinate basis for light-space aligned to Z-Up
    glm::vec3 lightRight = glm::cross(lightDir, glm::vec3(0.0f, 0.0f, 1.0f));
    if (glm::length2(lightRight) < 0.001f) {
        lightRight = glm::cross(lightDir, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    lightRight = glm::normalize(lightRight);
    glm::vec3 lightUp = glm::normalize(glm::cross(lightRight, lightDir));

    std::array<CascadeData, engineConfig::NUM_CASCADES> cascades{};
    for (int c = 0; c < engineConfig::NUM_CASCADES; c++) {
        float splitNear = splits[c];
        float splitFar = splits[c + 1];

        // 1. Find the stable bounding geometry for this sub-frustum split slice
        auto [sphereCenter, radius] = computeBoundingSphere(splitNear, splitFar, tanHalfFovX, tanHalfFovY, invCamView);

        // 2. Compute the final snapped, reverse-Z projection sequence for the GPU
        glm::mat4 lightSpaceMatrix = computeCascadeMatrix(sphereCenter, radius, lightDir, lightRight, lightUp);

        cascades[c] = {lightSpaceMatrix, splitFar};
    }

    return cascades;
}


// std::array<CascadeData, engineConfig::NUM_CASCADES>
// DirLightView::computeCascades(const glm::mat4 &cameraView, const glm::mat4 &cameraProj, float lambda) const {
//
//     // Extract camera params from projection matrix
//     float cameraNear = cameraProj[3][2];
//     float tanHalfFovY = std::abs(1.0f / cameraProj[1][1]);
//     float tanHalfFovX = 1.0f / cameraProj[0][0];
//
//     // Cascade split distances (positive, from camera)
//     std::array<float, engineConfig::NUM_CASCADES + 1> splits;
//     splits[0] = cameraNear;
//     splits[engineConfig::NUM_CASCADES] = shadowFar;
//     for (int i = 1; i < engineConfig::NUM_CASCADES; i++) {
//         float t = static_cast<float>(i) / static_cast<float>(engineConfig::NUM_CASCADES);
//         float logSplit = cameraNear * std::pow(shadowFar / cameraNear, t);
//         float uniformSplit = cameraNear + t * (shadowFar - cameraNear);
//         splits[i] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
//     }
//
//     // Extract the directional light's parallel orientation vector
//     glm::vec3 lightDir = glm::normalize(target - position);
//     glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f); // Standard Z-Up
//     if (glm::abs(glm::dot(lightDir, up)) > 0.999f)
//         up = glm::vec3(0.0f, 1.0f, 0.0f); // Fallback if light points straight down Z
//
//     // Create a stable rotation frame at the world origin to evaluate texel grids uniformly
//     glm::mat4 baseLightView = glm::lookAt(glm::vec3(0.0f), lightDir, up);
//     glm::mat4 invCamView = glm::inverse(cameraView);
//
//     std::array<CascadeData, engineConfig::NUM_CASCADES> cascades;
//     for (int c = 0; c < engineConfig::NUM_CASCADES; c++) {
//         float splitNear = splits[c];
//         float splitFar = splits[c + 1];
//
//         // Compute sub-frustum corners analytically in view space
//         float hN = splitNear * tanHalfFovY, wN = splitNear * tanHalfFovX;
//         float hF = splitFar * tanHalfFovY, wF = splitFar * tanHalfFovX;
//
//         std::array<glm::vec4, 8> viewCorners = {{
//             {-wN, -hN, -splitNear, 1.f}, {wN, -hN, -splitNear, 1.f},
//             {-wN, hN, -splitNear, 1.f}, {wN, hN, -splitNear, 1.f},
//             {-wF, -hF, -splitFar, 1.f}, {wF, -hF, -splitFar, 1.f},
//             {-wF, hF, -splitFar, 1.f}, {wF, hF, -splitFar, 1.f},
//         }};
//
//         // Convert corners to world space and compute the bounding sphere center
//         std::array<glm::vec3, 8> worldCorners;
//         glm::vec3 sphereCenter(0.0f);
//         for (int j = 0; j < 8; j++) {
//             worldCorners[j] = glm::vec3(invCamView * viewCorners[j]);
//             sphereCenter += worldCorners[j];
//         }
//         sphereCenter /= 8.0f;
//
//         // Calculate the bounding sphere radius
//         float radius = 0.0f;
//         for (const auto &wc : worldCorners)
//             radius = std::max(radius, glm::length(wc - sphereCenter));
//
//         // Round radius to prevent sub-pixel floating point scale variations
//         radius = std::ceil(radius * 16.0f) / 16.0f;
//
//         // Project the sphere center into our base light coordinate frame
//         glm::vec3 lightCenter = glm::vec3(baseLightView * glm::vec4(sphereCenter, 1.0f));
//
//         // Execute Texel Snapping: Lock the coordinates to discrete texel steps
//         float texelSize = (2.0f * radius) / static_cast<float>(engineConfig::SHADOW_MAP_SIZE);
//         lightCenter.x = std::floor(lightCenter.x / texelSize) * texelSize;
//         lightCenter.y = std::floor(lightCenter.y / texelSize) * texelSize;
//
//         // Unproject the snapped light-space coordinates back to Z-up world space
//         glm::vec3 snappedWorldCenter = glm::vec3(glm::inverse(baseLightView) * glm::vec4(lightCenter, 1.0f));
//
//         // Rebuild the dynamic, stable look-at matrix focused on the snapped world location
//         glm::vec3 cascadeLightPos = snappedWorldCenter - lightDir * radius;
//         glm::mat4 cascadeLightView = glm::lookAt(cascadeLightPos, snappedWorldCenter, up);
//
//         // Orthographic bounds are perfectly symmetric and constant relative to the center
//         float minX = -radius;
//         float maxX = radius;
//         float minY = -radius;
//         float maxY = radius;
//
//         // Compute stable Z extents using our new cascade view matrix frame
//         float minZ = std::numeric_limits<float>::max();
//         float maxZ = -std::numeric_limits<float>::max();
//         for (const auto &wc : worldCorners) {
//             float lz = (cascadeLightView * glm::vec4(wc, 1.0f)).z;
//             minZ = std::min(minZ, lz);
//             maxZ = std::max(maxZ, lz);
//         }
//
//         // Add a static buffer depth padding to enclose high structural casters outside the immediate view
//         float zNear = -maxZ - 50.0f;
//         float zFar = -minZ + 50.0f;
//
//         // Build the orthographic projection
//         glm::mat4 proj = glm::orthoRH_ZO(minX, maxX, minY, maxY, zNear, zFar);
//         proj[1][1] *= -1.0f; // Vulkan NDC Y-flip correction
//
//         // Reverse-Z: map depth [0, 1] -> [1, 0] for excellent bit precision
//         glm::mat4 revZ(1.0f);
//         revZ[2][2] = -1.0f;
//         revZ[3][2] = 1.0f;
//
//         // Combine using our stable, customized cascade view transformation matrix
//         cascades[c] = {revZ * proj * cascadeLightView, splitFar};
//     }
//
//     return cascades;
// }

// std::array<CascadeData, engineConfig::NUM_CASCADES> DirLightView::computeCascades(
//     const glm::mat4 &cameraView,
//     const glm::mat4 &cameraProj,
//     float lambda) const {
//
//     // Extract camera params from projection matrix (see Camera::getProjecsssssstionMatrix)
//     // proj[3][2] = cameraNear; proj[1][1] = -f (Y-flipped); proj[0][0] = f/aspect
//     float cameraNear  = cameraProj[3][2]; // near; no far term → infinite far plane
//     float tanHalfFovY = std::abs(1.0f / cameraProj[1][1]);
//     float tanHalfFovX = 1.0f / cameraProj[0][0];
//
//     // Cascade split distances (positive, from camera)
//     // Practical Split Scheme: blends logarithmic and uniform distributions
//     std::array<float, engineConfig::NUM_CASCADES + 1> splits;
//     splits[0] = cameraNear;
//     splits[engineConfig::NUM_CASCADES] = shadowFar;
//     for (int i = 1; i < engineConfig::NUM_CASCADES; i++) {
//         float t            = static_cast<float>(i) / static_cast<float>(engineConfig::NUM_CASCADES);
//         float logSplit     = cameraNear * std::pow(shadowFar / cameraNear, t);
//         float uniformSplit = cameraNear + t * (shadowFar - cameraNear);
//         splits[i] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
//     }
//
//     glm::mat4 lightView  = viewMatrix();
//     glm::mat4 invCamView = glm::inverse(cameraView);
//
//     std::array<CascadeData, engineConfig::NUM_CASCADES> cascades;
//     for (int c = 0; c < engineConfig::NUM_CASCADES; c++) {
//         float splitNear = splits[c];
//         float splitFar  = splits[c + 1];
//         /*
//          * instead of NDC unproject, we directly compute the 8 corner from view space using FOV
//          * This is cheaper and more numerically stable than NDC unproject — no matrix inversion of the projection matrix needed,
//          * no perspective divide.
//         */
//         // Sub-frustum corners in camera view space (camera at origin, looks down -Z, right-handed)
//         float hN = splitNear * tanHalfFovY,  wN = splitNear * tanHalfFovX;
//         float hF = splitFar  * tanHalfFovY,  wF = splitFar  * tanHalfFovX;
//
//         std::array<glm::vec4, 8> viewCorners = {{
//             {-wN, -hN, -splitNear, 1.f}, { wN, -hN, -splitNear, 1.f},
//             {-wN,  hN, -splitNear, 1.f}, { wN,  hN, -splitNear, 1.f},
//             {-wF, -hF, -splitFar,  1.f}, { wF, -hF, -splitFar,  1.f},
//             {-wF, hF, -splitFar, 1.f}, {wF, hF, -splitFar, 1.f},
//         }};
//
//         // Compute world-space corners and their bounding sphere.
//         // A sphere has a rotationally invariant radius, so texelSize = 2r/SHADOW_MAP_SIZE is
//         // constant regardless of camera orientation. This eliminates the AABB-width instability
//         // that causes shadow swimming when the camera rotates.
//         std::array<glm::vec3, 8> worldCorners;
//         glm::vec3 sphereCenter(0.0f);
//         for (int j = 0; j < 8; j++) {
//             worldCorners[j] = glm::vec3(invCamView * viewCorners[j]);
//             sphereCenter += worldCorners[j];
//         }
//         sphereCenter /= 8.0f;
//
//         float radius = 0.0f;
//         for (const auto &wc : worldCorners)
//             radius = std::max(radius, glm::length(wc - sphereCenter));
//
//         // Project each corner to light space for Z extents only.
//         float minZ = std::numeric_limits<float>::max();
//         float maxZ = -std::numeric_limits<float>::max();
//         for (const auto &wc : worldCorners) {
//             float lz = (lightView * glm::vec4(wc, 1.0f)).z;
//             minZ = std::min(minZ, lz);
//             maxZ = std::max(maxZ, lz);
//         }
//
//         // Texel-snap the sphere center in light space. texelSize is constant per frame so
//         // the snap grid is stable — shadows jump by exactly one texel, never drift.
//         glm::vec3 lightCenter = glm::vec3(lightView * glm::vec4(sphereCenter, 1.0f));
//         float texelSize = (2.0f * radius) / static_cast<float>(engineConfig::SHADOW_MAP_SIZE);
//         float cx = std::floor(lightCenter.x / texelSize) * texelSize;
//         float cy = std::floor(lightCenter.y / texelSize) * texelSize;
//
//         glm::vec3 minB(cx - radius, cy - radius, minZ);
//         glm::vec3 maxB(cx + radius, cy + radius, maxZ);
//
//         // Build orthographic projection from AABB.
//         // orthoRH_ZO maps view-space Z in [-zNear, -zFar] → NDC Z [0, 1].
//         // Light looks down -Z → closest objects are at maxB.z, farthest at minB.z.
//         float zNear = -maxB.z;
//         float zFar = -minB.z;
//
//         glm::mat4 proj = glm::orthoRH_ZO(minB.x, maxB.x, minB.y, maxB.y, zNear, zFar);
//         proj[1][1] *= -1.0f; // Vulkan Y-flip
//
//         // Reverse-Z: remap NDC Z [0,1] → [1,0] so near=1, far=0
//         glm::mat4 revZ(1.0f);
//         revZ[2][2] = -1.0f;
//         revZ[3][2] = 1.0f;
//
//         cascades[c] = {revZ * proj * lightView, splitFar};
//     }
//
//     return cascades;
// }