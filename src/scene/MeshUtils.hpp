//
// Created by johnny on 3/16/26.
//

#pragma once

#include <vector>
#include "../common/Vertex.hpp"

namespace MeshUtils {

void generateSphere(float radius, int sectorCount, int stackCount,
                    std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);

} // namespace MeshUtils
