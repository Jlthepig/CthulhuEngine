
#include "modelLoader.h"
#include "fastGltf/core.hpp"
#include "fastGltf/tools.hpp"
#include "fastGltf/glm_element_traits.hpp"
#include "fastGltf/types.hpp"
#include "fwd.hpp"
#include "log_utils.hpp"
#include "mesh.h"
#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Rendering
{
    Model ModelLoader::loadGltf(const std::string& path)
    {
        Model model;

        // recieve raw data from file and place it into a data buffer
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None)
        {
            Log::Print("FAILED TO LOAD GLTF FILE " + path,"ModelLoader", LogType::LOG_ERROR);
            return model;
        }

        // create  a parser and parse the raw data/file
        fastgltf::Parser parser;
        auto asset = parser.loadGltfBinary(data.get(),path);
        if (asset.error() != fastgltf::Error::None)
        {
            Log::Print("FAILED TO PARSE GLTF " + path,"ModelLoader", LogType::LOG_ERROR);
            return model;
        }


        fastgltf::Asset& gltf = asset.get();

        for (auto& mesh : gltf.meshes)
        {
            for (auto& primitive : mesh.primitives)
            {
                std::vector<float> vertexData;
                std::vector<unsigned int> indexdata;
                std::vector<Mesh::vertexAttribute> attributes;
                unsigned int currentOffset = 0;
                // find pos data
                auto* positionIt = primitive.findAttribute("POSITION");
                if (positionIt == primitive.attributes.end()) continue;

                auto& posAccessor = gltf.accessors[positionIt->accessorIndex];
                vertexData.resize(posAccessor.count * 3);
                
                  fastgltf::iterateAccessorWithIndex<glm::vec3>(
            gltf, posAccessor,
            [&](glm::vec3 pos, size_t index)
            {
                vertexData[index * 3 + 0] = pos.x;
                vertexData[index * 3 + 1] = pos.y;
                vertexData[index * 3 + 2] = pos.z;
            }
        );

                attributes.push_back({0,3,currentOffset});
                currentOffset +=3 *sizeof(float);

                // find normal data
                auto* normalIt = primitive.findAttribute("NORMAL");
                if (normalIt != primitive.attributes.end())
                {
                    auto& normAccessor  = gltf.accessors[normalIt->accessorIndex];
                    
                    // vertexdata has only space for pos so we need to expand to have space for normals too
                    // basically make 6 floats instead of 3, 3 for pos and 3 for normals
                    std:: vector<float> expanded(posAccessor.count * 6);

                    // get already existing pos and put into the new expanded layout
                    for (size_t i = 0; i< posAccessor.count;i++)
                    {
                        expanded[i * 6 + 0] = vertexData[i * 3 + 0];
                        expanded[i * 6 + +1] = vertexData[i * 3 + +1];
                        expanded[i * 6 + +2] = vertexData[i * 3 + +2];
                    }

                    // read the normals into the expanded layout

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        gltf, normAccessor, [&](glm::vec3 norm, size_t index)
                        {
                            expanded[index * 6 + 3] = norm.x;
                            expanded[index * 6 + 4] = norm.y;
                            expanded[index * 6 + 5] = norm.z;
                        }
                    );

                    vertexData = std::move(expanded);
                    attributes.push_back({1,3,currentOffset});
                    currentOffset += 3 * sizeof(float);
                }

                if (primitive.indicesAccessor.has_value())
                {
                    auto& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
                    indexdata.resize(indexAccessor.count);

                    fastgltf::iterateAccessorWithIndex<unsigned int>(
                        gltf, indexAccessor,[&](unsigned int idx, size_t index)
                    {
                        indexdata[index] = idx;
                    }
                    );

                   
                }

                 unsigned int stride = currentOffset;
                    Mesh newMesh;
                    newMesh.setup(vertexData, indexdata, attributes, stride);
                    model.meshes.push_back(std::move(newMesh));

            }
        }

        return model;
    };
}