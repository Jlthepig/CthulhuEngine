
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
#include <type_traits>
#include <utility>
#include <variant>
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
                    unsigned int currentFloats = currentOffset/sizeof(float);
                    unsigned int newFloats = currentFloats + 3;
                    // vertexdata has only space for pos so we need to expand to have space for normals too
                    // basically make 6 floats instead of 3, 3 for pos and 3 for normals
                    std:: vector<float> expanded(posAccessor.count * newFloats);

                    // get already existing pos and put into the new expanded layout
                    for (size_t i = 0; i< posAccessor.count;i++)
                    {
                        for (unsigned int j = 0; j<currentFloats;j++)
                            {
                                expanded[i * newFloats + j] = vertexData[i * currentFloats + j];

                            }
                    }

                    // read the normals into the expanded layout

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        gltf, normAccessor, [&](glm::vec3 norm, size_t index)
                        {
                            expanded[index * newFloats + currentFloats] = norm.x;
                            expanded[index * newFloats + currentFloats + 1] = norm.y;
                            expanded[index * newFloats + currentFloats + 2] = norm.z;
                        }
                    );

                    vertexData = std::move(expanded);
                    attributes.push_back({1,3,currentOffset});
                    currentOffset += 3 * sizeof(float);
                }

                auto* textureIt = primitive.findAttribute("TEXCOORD_0");
                auto* texture1It = primitive.findAttribute("TEXCOORD_1");
                Log::Print("Has TEXCOORD_0: " + std::to_string(textureIt != primitive.attributes.end()), "ModelLoader", LogType::LOG_INFO);
                Log::Print("Has TEXCOORD_1: " + std::to_string(texture1It != primitive.attributes.end()), "ModelLoader", LogType::LOG_INFO);
                    if (textureIt != primitive.attributes.end())
                    {
                        auto& uvAccessor = gltf.accessors[textureIt->accessorIndex];
                        // how many floats per vertex currently?
                        unsigned int currentFloats = currentOffset/sizeof(float);
                        unsigned int newFloats = currentFloats + 2;

                        std::vector<float> expanded(posAccessor.count * newFloats);

                        // copy the exisiting data into the new layout that supports texcoord
                        for(size_t i =0; i<posAccessor.count;i++)
                        {
                            for (unsigned int j = 0; j<currentFloats;j++)
                            {
                                expanded[i * newFloats + j] = vertexData[i * currentFloats + j];

                            }
                            
                        }

                        fastgltf::iterateAccessorWithIndex<glm::vec2>(
                            gltf, uvAccessor, [&](glm::vec2 uv , size_t index)
                        {
                            expanded[index * newFloats + currentFloats] = uv.x;
                            expanded[index * newFloats + currentFloats + 1] = uv.y;
                        }
                        
                    );

                    vertexData = std::move(expanded);
                    attributes.push_back({2,2,currentOffset});
                    currentOffset += 2 * sizeof(float);
                        
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
                    Log::Print("Vertex count: " + std::to_string(posAccessor.count), "ModelLoader", LogType::LOG_INFO);
                    Log::Print("Index count: " + std::to_string(indexdata.size()), "ModelLoader", LogType::LOG_INFO);
                    Log::Print("Stride: " + std::to_string(stride), "ModelLoader", LogType::LOG_INFO);
                    Log::Print("Attributes: " + std::to_string(attributes.size()), "ModelLoader", LogType::LOG_INFO);

                    Mesh newMesh;
                    newMesh.setup(vertexData, indexdata, attributes, stride);

                    Texture newTexture;
                    if (primitive.materialIndex.has_value())
                    {
                        auto& material = gltf.materials[primitive.materialIndex.value()];
                        if (material.pbrData.baseColorTexture.has_value())
                        {
                            auto& textureInfo = material.pbrData.baseColorTexture.value();
                            auto& texture = gltf.textures[textureInfo.textureIndex];

                            if (texture.imageIndex.has_value())
                            {
                                auto image = gltf.images[texture.imageIndex.value()];
                                
                                std::visit([&](auto& source)
                                {
                                    using T = std::decay_t<decltype(source)>;
                                    if constexpr (std::is_same_v<T, fastgltf::sources::BufferView>)
                                    {
                                        auto& bufferView = gltf.bufferViews[source.bufferViewIndex];
                                        auto& buffer = gltf.buffers[bufferView.bufferIndex];

                                        std::visit([&](auto& bufferSource)
                                        {
                                            using BT = std::decay_t<decltype(bufferSource)>;
                                            if constexpr (std::is_same_v<BT, fastgltf::sources::Array>)
                                            {

                                                const unsigned char* data = reinterpret_cast<const unsigned char*>(bufferSource.bytes.data() + bufferView.byteOffset);
                                                newTexture.loadFromMemory(data,static_cast<int>(bufferView.byteLength));
                                            
                                            }
                                        }, buffer.data);
                                    }
                                    
                                    else if constexpr (std::is_same_v<T, fastgltf::sources::URI>)
                                    {
                                        Log::Print("TEXTURE IS URI - NOT SUPPORTED YET", "ModelLoader", LogType::LOG_WARNING);
                                    }
                                    else
                                    {
                                        Log::Print("TEXTURE FORMAT NOT HANDLED", "ModelLoader", LogType::LOG_WARNING);
                                    }
                                    
                                },image.data);
                            }
                        }
                    }
                    model.textures.push_back(std::move(newTexture));
                    model.meshes.push_back(std::move(newMesh));

            }
        }
        Log::Print("Meshes: " + std::to_string(model.meshes.size()) + " Textures: " + std::to_string(model.textures.size()), "ModelLoader", LogType::LOG_INFO);
        return model;
    };
}