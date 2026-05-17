//
// Created by johnny on 5/11/26.
//

#include "DirLightView.hpp"
#include <cmath>
#include <limits>

std::array<CascadeData, engineConfig::NUM_CASCADES> DirLightView::computeCascades(
    const glm::mat4 &cameraView,
    const glm::mat4 &cameraProj,
    float lambda) const {

    // Extract camera params from projection matrix (see Camera::getProjectionMatrix)
    // proj[3][2] = cameraNear; proj[1][1] = -f (Y-flipped); proj[0][0] = f/aspect
    float cameraNear  = cameraProj[3][2]; // near; no far term → infinite far plane
    float tanHalfFovY = std::abs(1.0f / cameraProj[1][1]);
    float tanHalfFovX = 1.0f / cameraProj[0][0];

    // Cascade split distances (positive, from camera)
    // Practical Split Scheme: blends logarithmic and uniform distributions
    std::array<float, engineConfig::NUM_CASCADES + 1> splits;
    splits[0] = cameraNear;
    splits[engineConfig::NUM_CASCADES] = shadowFar;
    for (int i = 1; i < engineConfig::NUM_CASCADES; i++) {
        float t            = static_cast<float>(i) / static_cast<float>(engineConfig::NUM_CASCADES);
        float logSplit     = cameraNear * std::pow(shadowFar / cameraNear, t);
        float uniformSplit = cameraNear + t * (shadowFar - cameraNear);
        splits[i] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
    }

    glm::mat4 lightView  = viewMatrix();
    glm::mat4 invCamView = glm::inverse(cameraView);

    std::array<CascadeData, engineConfig::NUM_CASCADES> cascades;
    for (int c = 0; c < engineConfig::NUM_CASCADES; c++) {
        float splitNear = splits[c];
        float splitFar  = splits[c + 1];
        /*
         * instead of NDC unproject, we directly compute the 8 corner from view space using FOV
         * This is cheaper and more numerically stable than NDC unproject — no matrix inversion of the projection matrix needed,
         * no perspective divide.
        */
        // Sub-frustum corners in camera view space (camera at origin, looks down -Z, right-handed)
        float hN = splitNear * tanHalfFovY,  wN = splitNear * tanHalfFovX;
        float hF = splitFar  * tanHalfFovY,  wF = splitFar  * tanHalfFovX;

        std::array<glm::vec4, 8> viewCorners = {{
            {-wN, -hN, -splitNear, 1.f}, { wN, -hN, -splitNear, 1.f},
            {-wN,  hN, -splitNear, 1.f}, { wN,  hN, -splitNear, 1.f},
            {-wF, -hF, -splitFar,  1.f}, { wF, -hF, -splitFar,  1.f},
            {-wF, hF, -splitFar, 1.f}, {wF, hF, -splitFar, 1.f},
        }};

        // Compute world-space corners and their bounding sphere.
        // A sphere has a rotationally invariant radius, so texelSize = 2r/SHADOW_MAP_SIZE is
        // constant regardless of camera orientation. This eliminates the AABB-width instability
        // that causes shadow swimming when the camera rotates.
        std::array<glm::vec3, 8> worldCorners;
        glm::vec3 sphereCenter(0.0f);
        for (int j = 0; j < 8; j++) {
            worldCorners[j] = glm::vec3(invCamView * viewCorners[j]);
            sphereCenter += worldCorners[j];
        }
        sphereCenter /= 8.0f;

        float radius = 0.0f;
        for (const auto &wc : worldCorners)
            radius = std::max(radius, glm::length(wc - sphereCenter));

        // Project each corner to light space for Z extents only.
        float minZ = std::numeric_limits<float>::max();
        float maxZ = -std::numeric_limits<float>::max();
        for (const auto &wc : worldCorners) {
            float lz = (lightView * glm::vec4(wc, 1.0f)).z;
            minZ = std::min(minZ, lz);
            maxZ = std::max(maxZ, lz);
        }

        // Texel-snap the sphere center in light space. texelSize is constant per frame so
        // the snap grid is stable — shadows jump by exactly one texel, never drift.
        glm::vec3 lightCenter = glm::vec3(lightView * glm::vec4(sphereCenter, 1.0f));
        float texelSize = (2.0f * radius) / static_cast<float>(engineConfig::SHADOW_MAP_SIZE);
        float cx = std::floor(lightCenter.x / texelSize) * texelSize;
        float cy = std::floor(lightCenter.y / texelSize) * texelSize;

        glm::vec3 minB(cx - radius, cy - radius, minZ);
        glm::vec3 maxB(cx + radius, cy + radius, maxZ);

        // Build orthographic projection from AABB.
        // orthoRH_ZO maps view-space Z in [-zNear, -zFar] → NDC Z [0, 1].
        // Light looks down -Z → closest objects are at maxB.z, farthest at minB.z.
        float zNear = -maxB.z;
        float zFar = -minB.z;

        glm::mat4 proj = glm::orthoRH_ZO(minB.x, maxB.x, minB.y, maxB.y, zNear, zFar);
        proj[1][1] *= -1.0f; // Vulkan Y-flip

        // Reverse-Z: remap NDC Z [0,1] → [1,0] so near=1, far=0
        glm::mat4 revZ(1.0f);
        revZ[2][2] = -1.0f;
        revZ[3][2] = 1.0f;

        cascades[c] = {revZ * proj * lightView, splitFar};
    }

    return cascades;
}