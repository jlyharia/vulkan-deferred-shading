//
// Created by johnny on 2/27/26.
//

#pragma once
#include "Transform.hpp"
#include <memory>
class Model;

struct RenderObject {
    std::shared_ptr<Model> model;
    Transform transform;
    std::string name;
};