//
// Created by johnny on 2/27/26.
//

#include "AssetManager.hpp"

#include "scene/Model.hpp"


std::shared_ptr<Model> AssetManager::loadModel(const std::string &path) {
    if (modelCache_.contains(path)) {
        return modelCache_[path];
    }

    auto model = std::make_shared<Model>(context_.getVmaAllocator());
    model->loadFromFile(path);

    // Use the pool and queue to move data from CPU RAM to GPU VRAM
    model->uploadToGPU(context_.getDevice(), context_.getGraphicsQueue(), transferPool_);

    modelCache_[path] = model;
    return model;
}