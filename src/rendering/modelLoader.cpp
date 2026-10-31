#include <cfloat>
#include <cstddef>
#include <numeric>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <glm.hpp>
#include <gtc/matrix_inverse.hpp>

#include "fastgltf/tools.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/types.hpp>

#include "log_utils.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "modelLoader.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Rendering
{
std::optional<Model> ModelLoader::loadGltf(const std::string &path)
{
    Model model;

    // recieve raw data from file and place it into a data buffer
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None)
    {
        Log::Print("FAILED TO LOAD GLTF FILE " + path, "ModelLoader", LogType::LOG_ERROR);
        return std::nullopt;
    }

    // create  a parser and parse the raw data/file
    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_unlit | fastgltf::Extensions::KHR_texture_transform |
                            fastgltf::Extensions::KHR_materials_specular | fastgltf::Extensions::KHR_materials_ior |
                            fastgltf::Extensions::KHR_materials_emissive_strength |
                            fastgltf::Extensions::KHR_materials_sheen |
                            fastgltf::Extensions::KHR_materials_transmission |
                            fastgltf::Extensions::KHR_materials_volume | fastgltf::Extensions::KHR_materials_clearcoat);
    auto asset = parser.loadGltfBinary(data.get(), path);
    if (asset.error() != fastgltf::Error::None)
    {
        Log::Print("FAILED TO PARSE GLTF " + path + " Error: " + std::string(fastgltf::getErrorMessage(asset.error())),
                   "ModelLoader",
                   LogType::LOG_ERROR);
        return std::nullopt;
    }

    fastgltf::Asset &gltf = asset.get();
    // pass 1 load materials and textures
    std::unordered_map<int, int> imageToTexture; // from gltf texture index to our texture index in
    // model.textures

    for (size_t matIdx = 0; matIdx < gltf.materials.size(); ++matIdx)
    {
        auto &gltfMaterial = gltf.materials[matIdx];
        Material material;

        // base color factor default to white
        material.baseColorFactor = glm::vec4(gltfMaterial.pbrData.baseColorFactor[0],
                                             gltfMaterial.pbrData.baseColorFactor[1],
                                             gltfMaterial.pbrData.baseColorFactor[2],
                                             gltfMaterial.pbrData.baseColorFactor[3]);

        if (gltfMaterial.pbrData.baseColorTexture.has_value())
        {
            auto &textureInfo = gltfMaterial.pbrData.baseColorTexture.value();
            auto &gltfTexture = gltf.textures[textureInfo.textureIndex];

            if (gltfTexture.imageIndex.has_value())
            {
                int imageIdx = static_cast<int>(gltfTexture.imageIndex.value());

                // check if we already loaded this specific texture
                auto it = imageToTexture.find(imageIdx);
                if (it != imageToTexture.end())
                {
                    // reuse  a existing texture
                    material.baseColorTextureIndex = it->second;
                }
                else
                {
                    auto &image = gltf.images[imageIdx];
                    Texture newTexture;

                    std::visit(
                        [&](auto &source) {
                            using T = std::decay_t<decltype(source)>;
                            if constexpr (std::is_same_v<T, fastgltf::sources::BufferView>)
                            {
                                auto &bufferView = gltf.bufferViews[source.bufferViewIndex];
                                auto &buffer = gltf.buffers[bufferView.bufferIndex];

                                std::visit(
                                    [&](auto &bufferSource) {
                                        using BT = std::decay_t<decltype(bufferSource)>;
                                        if constexpr (std::is_same_v<BT, fastgltf::sources::Array>)
                                        {
                                            const unsigned char *data = reinterpret_cast<const unsigned char *>(
                                                bufferSource.bytes.data() + bufferView.byteOffset);
                                            newTexture.loadFromMemory(data, static_cast<int>(bufferView.byteLength));
                                        }
                                    },
                                    buffer.data);
                            }

                            else if constexpr (std::is_same_v<T, fastgltf::sources::URI>)
                            {
                                Log::Print("TEXTURE IS URI - NOT SUPPORTED YET", "ModelLoader", LogType::LOG_WARNING);
                            }
                            else
                            {
                                Log::Print("TEXTURE FORMAT NOT HANDLED", "ModelLoader", LogType::LOG_WARNING);
                            }
                        },
                        image.data);

                    int textureIndex = static_cast<int>(model.textures.size());
                    model.textures.push_back(std::move(newTexture));
                    imageToTexture[imageIdx] = textureIndex;
                    material.baseColorTextureIndex = textureIndex;
                }
            }
        }
        material.metallicFactor = gltfMaterial.pbrData.metallicFactor;
        material.roughnessFactor = gltfMaterial.pbrData.roughnessFactor;

        if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value())
        {
            auto &mrTextureInfo = gltfMaterial.pbrData.metallicRoughnessTexture.value();
            auto &gltfTexture = gltf.textures[mrTextureInfo.textureIndex];

            if (gltfTexture.imageIndex.has_value())
            {
                int imageIdx = static_cast<int>(gltfTexture.imageIndex.value());

                auto it = imageToTexture.find(imageIdx);
                if (it != imageToTexture.end())
                {
                    material.metallicRoughnessTextureIndex = it->second;
                }
                else
                {
                    auto &image = gltf.images[imageIdx];
                    Texture newTexture;

                    std::visit(
                        [&](auto &source) {
                            using T = std::decay_t<decltype(source)>;
                            if constexpr (std::is_same_v<T, fastgltf::sources::BufferView>)
                            {
                                auto &bufferView = gltf.bufferViews[source.bufferViewIndex];
                                auto &buffer = gltf.buffers[bufferView.bufferIndex];

                                std::visit(
                                    [&](auto &bufferSource) {
                                        using BT = std::decay_t<decltype(bufferSource)>;
                                        if constexpr (std::is_same_v<BT, fastgltf::sources::Array>)
                                        {
                                            const unsigned char *data = reinterpret_cast<const unsigned char *>(
                                                bufferSource.bytes.data() + bufferView.byteOffset);
                                            newTexture.loadFromMemory(data,
                                                                      static_cast<int>(bufferView.byteLength),
                                                                      false);
                                        }
                                    },
                                    buffer.data);
                            }

                            else if constexpr (std::is_same_v<T, fastgltf::sources::URI>)
                            {
                                Log::Print("METALLIC/ROUGHNESS URI - NOT SUPPORTED YET",
                                           "ModelLoader",
                                           LogType::LOG_WARNING);
                            }
                            else
                            {
                                Log::Print("METALLIC/ROUGHNESS TEXTURE FORMAT NOT HANDLED",
                                           "ModelLoader",
                                           LogType::LOG_WARNING);
                            }
                        },
                        image.data);

                    int textureIndex = static_cast<int>(model.textures.size());
                    model.textures.push_back(std::move(newTexture));
                    imageToTexture[imageIdx] = textureIndex;
                    material.metallicRoughnessTextureIndex = textureIndex;
                }
            }
        }
        if (gltfMaterial.normalTexture.has_value())
        {
            auto &normTextureInfo = gltfMaterial.normalTexture.value();
            auto &gltfTexture = gltf.textures[normTextureInfo.textureIndex];

            if (gltfTexture.imageIndex.has_value())
            {
                int imageIdx = static_cast<int>(gltfTexture.imageIndex.value());

                auto it = imageToTexture.find(imageIdx);
                if (it != imageToTexture.end())
                {
                    material.normalTextureIndex = it->second;
                }
                else
                {
                    auto &image = gltf.images[imageIdx];
                    Texture newTexture;

                    std::visit(
                        [&](auto &source) {
                            using T = std::decay_t<decltype(source)>;
                            if constexpr (std::is_same_v<T, fastgltf::sources::BufferView>)
                            {
                                auto &bufferView = gltf.bufferViews[source.bufferViewIndex];
                                auto &buffer = gltf.buffers[bufferView.bufferIndex];

                                std::visit(
                                    [&](auto &bufferSource) {
                                        using BT = std::decay_t<decltype(bufferSource)>;
                                        if constexpr (std::is_same_v<BT, fastgltf::sources::Array>)
                                        {
                                            const unsigned char *data = reinterpret_cast<const unsigned char *>(
                                                bufferSource.bytes.data() + bufferView.byteOffset);
                                            newTexture.loadFromMemory(data,
                                                                      static_cast<int>(bufferView.byteLength),
                                                                      false);
                                        }
                                    },
                                    buffer.data);
                            }
                        },
                        image.data);

                    int textureIndex = static_cast<int>(model.textures.size());
                    model.textures.push_back(std::move(newTexture));
                    imageToTexture[imageIdx] = textureIndex;
                    material.normalTextureIndex = textureIndex;
                }
            }
        }

        model.materials.push_back(std::move(material));
    }

    // pass 2 load meshes
    glm::vec3 modelMin(FLT_MAX);
    glm::vec3 modelMax(-FLT_MAX);

    auto appendMesh = [&](std::size_t meshIndex, const glm::mat4 &worldTransform) {
        auto &mesh = gltf.meshes[meshIndex];
        const glm::mat3 linearTransform(worldTransform);
        const float determinant = glm::determinant(linearTransform);
        const bool mirrored = determinant < 0.0f;
        const glm::mat3 normalTransform =
            std::abs(determinant) > FLT_EPSILON ? glm::transpose(glm::inverse(linearTransform)) : glm::mat3(1.0f);

        for (auto &primitive : mesh.primitives)
        {
            std::vector<float> vertexData;
            std::vector<unsigned int> indexdata;
            std::vector<Mesh::vertexAttribute> attributes;
            unsigned int currentOffset = 0;
            // find pos data
            auto *positionIt = primitive.findAttribute("POSITION");
            if (positionIt == primitive.attributes.end())
                continue;

            auto &posAccessor = gltf.accessors[positionIt->accessorIndex];
            vertexData.resize(posAccessor.count * 3);

            fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, [&](glm::vec3 pos, size_t index) {
                const glm::vec3 worldPosition = glm::vec3(worldTransform * glm::vec4(pos, 1.0f));
                modelMin = glm::min(modelMin, worldPosition);
                modelMax = glm::max(modelMax, worldPosition);

                vertexData[index * 3 + 0] = worldPosition.x;
                vertexData[index * 3 + 1] = worldPosition.y;
                vertexData[index * 3 + 2] = worldPosition.z;
            });

            attributes.push_back({0, 3, currentOffset});
            currentOffset += 3 * sizeof(float);

            // find normal data
            auto *normalIt = primitive.findAttribute("NORMAL");
            if (normalIt != primitive.attributes.end())
            {
                auto &normAccessor = gltf.accessors[normalIt->accessorIndex];
                unsigned int currentFloats = currentOffset / sizeof(float);
                unsigned int newFloats = currentFloats + 3;
                // vertexdata has only space for pos so we need to expand to have space
                // for normals too basically make 6 floats instead of 3, 3 for pos and 3
                // for normals
                std::vector<float> expanded(posAccessor.count * newFloats);

                // get already existing pos and put into the new expanded layout
                for (size_t i = 0; i < posAccessor.count; i++)
                {
                    for (unsigned int j = 0; j < currentFloats; j++)
                    {
                        expanded[i * newFloats + j] = vertexData[i * currentFloats + j];
                    }
                }

                // read the normals into the expanded layout

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, normAccessor, [&](glm::vec3 norm, size_t index) {
                    const glm::vec3 worldNormal = glm::normalize(normalTransform * norm);
                    expanded[index * newFloats + currentFloats] = worldNormal.x;
                    expanded[index * newFloats + currentFloats + 1] = worldNormal.y;
                    expanded[index * newFloats + currentFloats + 2] = worldNormal.z;
                });

                vertexData = std::move(expanded);
                attributes.push_back({1, 3, currentOffset});
                currentOffset += 3 * sizeof(float);
            }

            auto tangentIt = primitive.findAttribute("TANGENT");
            if (tangentIt != primitive.attributes.end())
            {
                auto &tanAccessor = gltf.accessors[tangentIt->accessorIndex];
                unsigned int currentFloats = currentOffset / sizeof(float);
                unsigned int newFloats = currentFloats + 4; // tangents are vec4
                std::vector<float> expanded(posAccessor.count * newFloats);

                // copy existing data
                for (size_t i = 0; i < posAccessor.count; i++)
                {
                    for (unsigned int j = 0; j < currentFloats; j++)
                    {
                        expanded[i * newFloats + j] = vertexData[i * currentFloats + j];
                    }
                }

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, tanAccessor, [&](glm::vec4 tan, size_t index) {
                    const glm::vec3 worldTangent = glm::normalize(linearTransform * glm::vec3(tan));
                    expanded[index * newFloats + currentFloats] = worldTangent.x;
                    expanded[index * newFloats + currentFloats + 1] = worldTangent.y;
                    expanded[index * newFloats + currentFloats + 2] = worldTangent.z;
                    expanded[index * newFloats + currentFloats + 3] = mirrored ? -tan.w : tan.w;
                });

                vertexData = std::move(expanded);
                attributes.push_back({3, 4, currentOffset});
                currentOffset += 4 * sizeof(float);
            }

            auto *textureIt = primitive.findAttribute("TEXCOORD_0");
            auto *texture1It = primitive.findAttribute("TEXCOORD_1");
            Log::Print("Has TEXCOORD_0: " + std::to_string(textureIt != primitive.attributes.end()),
                       "ModelLoader",
                       LogType::LOG_INFO);
            Log::Print("Has TEXCOORD_1: " + std::to_string(texture1It != primitive.attributes.end()),
                       "ModelLoader",
                       LogType::LOG_INFO);
            if (textureIt != primitive.attributes.end())
            {
                auto &uvAccessor = gltf.accessors[textureIt->accessorIndex];
                // how many floats per vertex currently?
                unsigned int currentFloats = currentOffset / sizeof(float);
                unsigned int newFloats = currentFloats + 2;

                std::vector<float> expanded(posAccessor.count * newFloats);

                // copy the exisiting data into the new layout that supports texcoord
                for (size_t i = 0; i < posAccessor.count; i++)
                {
                    for (unsigned int j = 0; j < currentFloats; j++)
                    {
                        expanded[i * newFloats + j] = vertexData[i * currentFloats + j];
                    }
                }

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf,
                                                              uvAccessor,
                                                              [&](glm::vec2 uv, size_t index) {
                                                                  expanded[index * newFloats + currentFloats] = uv.x;
                                                                  expanded[index * newFloats + currentFloats + 1] =
                                                                      uv.y;
                                                              }

                );

                vertexData = std::move(expanded);
                attributes.push_back({2, 2, currentOffset});
                currentOffset += 2 * sizeof(float);
            }

            if (primitive.indicesAccessor.has_value())
            {
                auto &indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
                indexdata.resize(indexAccessor.count);

                fastgltf::iterateAccessorWithIndex<unsigned int>(gltf,
                                                                 indexAccessor,
                                                                 [&](unsigned int idx, size_t index) {
                                                                     indexdata[index] = idx;
                                                                 });
            }
            else
            {
                indexdata.resize(posAccessor.count);
                std::iota(indexdata.begin(), indexdata.end(), 0u);
            }

            if (mirrored && primitive.type == fastgltf::PrimitiveType::Triangles)
            {
                for (std::size_t index = 0; index + 2 < indexdata.size(); index += 3)
                {
                    std::swap(indexdata[index + 1], indexdata[index + 2]);
                }
            }

            unsigned int stride = currentOffset;
            Mesh newMesh;
            newMesh.setup(vertexData, indexdata, attributes, stride);

            Texture newTexture;
            if (primitive.materialIndex.has_value())
            {
                int gltfMatIdx = static_cast<int>(primitive.materialIndex.value());
                if (gltfMatIdx >= 0 && gltfMatIdx < static_cast<int>(model.materials.size()))
                {
                    newMesh.materialIndex = gltfMatIdx;
                }
            }

            model.meshes.push_back(std::move(newMesh));
        }
    };

    if (gltf.scenes.empty())
    {
        Log::Print("GLTF CONTAINS NO SCENES: " + path, "ModelLoader", LogType::LOG_ERROR);
        model.destroy();
        return std::nullopt;
    }

    const std::size_t sceneIndex = gltf.defaultScene.has_value() ? gltf.defaultScene.value() : 0;
    if (sceneIndex >= gltf.scenes.size())
    {
        Log::Print("GLTF DEFAULT SCENE INDEX IS INVALID: " + path, "ModelLoader", LogType::LOG_ERROR);
        model.destroy();
        return std::nullopt;
    }

    fastgltf::iterateSceneNodes(
        gltf,
        sceneIndex,
        fastgltf::math::fmat4x4(),
        [&](fastgltf::Node &node, const fastgltf::math::fmat4x4 &nodeTransform) {
            if (!node.meshIndex.has_value())
            {
                return;
            }

            glm::mat4 worldTransform(1.0f);
            for (glm::length_t column = 0; column < 4; ++column)
            {
                for (glm::length_t row = 0; row < 4; ++row)
                {
                    worldTransform[column][row] =
                        nodeTransform[static_cast<std::size_t>(column)][static_cast<std::size_t>(row)];
                }
            }
            appendMesh(node.meshIndex.value(), worldTransform);
        });

    if (modelMin.x <= modelMax.x)
    {
        model.boundsMin = modelMin;
        model.boundsMax = modelMax;
    }

    if (model.meshes.empty())
    {
        Log::Print("GLTF CONTAINS NO USABLE MESHES: " + path, "Modelloader", LogType::LOG_ERROR);
        model.destroy();
        return std::nullopt;
    }

    return model;
};
} // namespace Cthulhu::Rendering
