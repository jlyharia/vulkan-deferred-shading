//
// Created by johnny on 1/24/26.
//

#pragma once
#include <memory>
#include <string>


class ModelSystem
{
public:
    // This function will use tinygltf to load vertices into VMA buffers
    // std::shared_ptr<ModelData> loadModel(const std::string& path);
    void loadModel(const std::string& filePath){}
    void loadObjModel(const std::string& filePath);
};
