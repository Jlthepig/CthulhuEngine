#include <cfloat>
#include <cstddef>
#include <numeric>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>

#include <glm.hpp>
#include <gtc/matrix_inverse.hpp>

#include "fastgltf/tools.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/types.hpp>

#include "material.hpp"
#include "mesh.hpp"
#include "modelLoader.hpp"
#include "pathEncoding.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Rendering
{

static std::span<const std::byte> getResidentBytes(const fastgltf::DataSource &source)
{
    return std::visit(
        [](const auto &data) -> std::span<const std::byte> {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, fastgltf::sources::Array> ||
                          std::is_same_v<T, fastgltf::sources::Vector> ||
                          std::is_same_v<T, fastgltf::sources::ByteView>)
            {
                return {data.bytes.data(), data.bytes.size()};
            }

            return {};
        },
        source);
}

static std::span<const std::byte> getBufferViewBytes(const fastgltf::Asset &asset, std::size_t viewIndex)
{
    if (viewIndex >= asset.bufferViews.size())
        return {};

    const auto &view = asset.bufferViews[viewIndex];
    if (view.bufferIndex >= asset.buffers.size() || view.meshoptCompression)
        return {};

    const auto &buffer = asset.buffers[view.bufferIndex];
    const auto bytes = getResidentBytes(buffer.data);

    if (view.byteLength == 0 ||
        view.byteOffset > buffer.byteLength ||
        view.byteLength > buffer.byteLength - view.byteOffset ||
        view.byteOffset > bytes.size() ||
        view.byteLength > bytes.size() - view.byteOffset)
    {
        return {};
    }

    return bytes.subspan(view.byteOffset, view.byteLength);
}

static bool rangeFits(std::size_t byteLength, std::size_t byteOffset, std::size_t count, std::size_t stride, std::size_t elementBytes)
{
    return count != 0 &&
           elementBytes != 0 &&
           stride >= elementBytes &&
           byteOffset <= byteLength &&
           elementBytes <= byteLength - byteOffset &&
           count - 1 <= (byteLength - byteOffset - elementBytes) / stride;
}

static bool validateAccessor(const fastgltf::Asset &asset, std::size_t accessorIndex, fastgltf::AccessorType expectedType)
{
    if (accessorIndex >= asset.accessors.size())
        return false;

    const auto &accessor = asset.accessors[accessorIndex];
    if (accessor.type != expectedType || accessor.count == 0)
        return false;

    switch (accessor.componentType)
    {
    case fastgltf::ComponentType::Byte:
    case fastgltf::ComponentType::UnsignedByte:
    case fastgltf::ComponentType::Short:
    case fastgltf::ComponentType::UnsignedShort:
    case fastgltf::ComponentType::UnsignedInt:
    case fastgltf::ComponentType::Float:
        break;
    default:
        return false;
    }

    const std::size_t componentBytes = fastgltf::getComponentByteSize(accessor.componentType);
    const std::size_t elementBytes = fastgltf::getElementByteSize(accessor.type, accessor.componentType);

    if (accessor.bufferViewIndex.has_value())
    {
        const auto bytes = getBufferViewBytes(asset, accessor.bufferViewIndex.value());
        if (bytes.empty())
            return false;

        const auto &view = asset.bufferViews[accessor.bufferViewIndex.value()];
        const std::size_t stride = view.byteStride.value_or(elementBytes);

        if (accessor.byteOffset % componentBytes != 0 ||
            view.byteOffset % componentBytes != 0 ||
            stride % componentBytes != 0 ||
            (view.byteStride.has_value() &&
             (stride < 4 || stride > 252 || stride % 4 != 0)) ||
            !rangeFits(bytes.size(), accessor.byteOffset,
                       accessor.count, stride, elementBytes))
        {
            return false;
        }
    }
    else if (accessor.byteOffset != 0)
    {
        return false;
    }

    if (!accessor.sparse.has_value())
        return true;

    const auto &sparse = accessor.sparse.value();
    if (sparse.count == 0 || sparse.count > accessor.count)
        return false;

    if (sparse.indexComponentType != fastgltf::ComponentType::UnsignedByte &&
        sparse.indexComponentType != fastgltf::ComponentType::UnsignedShort &&
        sparse.indexComponentType != fastgltf::ComponentType::UnsignedInt)
    {
        return false;
    }

    const auto indices = getBufferViewBytes(asset, sparse.indicesBufferView);
    const auto values = getBufferViewBytes(asset, sparse.valuesBufferView);
    if (indices.empty() || values.empty())
        return false;

    const auto &indexView = asset.bufferViews[sparse.indicesBufferView];
    const auto &valueView = asset.bufferViews[sparse.valuesBufferView];
    const std::size_t indexBytes = fastgltf::getComponentByteSize(sparse.indexComponentType);

    if (indexView.byteStride.has_value() || indexView.target.has_value() ||
        valueView.byteStride.has_value() || valueView.target.has_value() ||
        sparse.indicesByteOffset % indexBytes != 0 ||
        indexView.byteOffset % indexBytes != 0 ||
        sparse.valuesByteOffset % componentBytes != 0 ||
        valueView.byteOffset % componentBytes != 0 ||
        !rangeFits(indices.size(), sparse.indicesByteOffset,
                   sparse.count, indexBytes, indexBytes) ||
        !rangeFits(values.size(), sparse.valuesByteOffset,
                   sparse.count, elementBytes, elementBytes))
    {
        return false;
    }

    std::uint32_t previous{};

    for (std::size_t i = 0; i < sparse.count; ++i)
    {
        const std::size_t offset = sparse.indicesByteOffset + i * indexBytes;
        std::uint32_t index = 0;

        for (std::size_t byte = 0; byte < indexBytes; ++byte)
        {
            index |= std::to_integer<std::uint32_t>(indices[offset + byte]) << (8 * byte);
        }

        if (index >= accessor.count || (i != 0 && index <= previous))
            return false;

        previous = index;
    }

    return true;
}

static bool validateGeometry(const fastgltf::Asset &asset)
{
    struct AttributeRule
    {
        const char *name;
        fastgltf::AccessorType type;
        bool floatOnly;
    };

    const AttributeRule rules[] = {
        {"POSITION", fastgltf::AccessorType::Vec3, true},
        {"NORMAL", fastgltf::AccessorType::Vec3, true},
        {"TANGENT", fastgltf::AccessorType::Vec4, true},
        {"TEXCOORD_0", fastgltf::AccessorType::Vec2, false},
    };

    const std::size_t maximumIndices = std::min({
        static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()),
        static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) /
            sizeof(unsigned int),
        std::vector<unsigned int>{}.max_size(),
    });

    if (asset.materials.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return false;
    }

    for (const auto &mesh : asset.meshes)
    {
        for (const auto &primitive : mesh.primitives)
        {
            const auto *position = primitive.findAttribute("POSITION");
            if (position == primitive.attributes.end() ||
                position->accessorIndex >= asset.accessors.size())
            {
                return false;
            }

            const std::size_t vertexCount = asset.accessors[position->accessorIndex].count;

            if (vertexCount == 0 || vertexCount > std::numeric_limits<unsigned int>::max())
            {
                return false;
            }

            std::size_t floatsPerVertex = 0;

            for (const auto &rule : rules)
            {
                const auto *attribute = primitive.findAttribute(rule.name);
                if (attribute == primitive.attributes.end())
                    continue;

                if (!validateAccessor(asset, attribute->accessorIndex, rule.type))
                    return false;

                const auto &accessor = asset.accessors[attribute->accessorIndex];
                if (accessor.count != vertexCount)
                    return false;

                const bool floatData = accessor.componentType == fastgltf::ComponentType::Float && !accessor.normalized;

                const bool normalizedUV = accessor.normalized && (accessor.componentType == fastgltf::ComponentType::UnsignedByte || 
                                                                accessor.componentType == fastgltf::ComponentType::UnsignedShort);

                if (!floatData && (rule.floatOnly || !normalizedUV))
                    return false;

                floatsPerVertex += fastgltf::getNumComponents(rule.type);
            }

            const std::size_t maximumVertices = std::min(
                std::vector<float>{}.max_size() / floatsPerVertex,
                static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) /
                    (floatsPerVertex * sizeof(float)));

            if (vertexCount > maximumVertices)
                return false;

            if (primitive.materialIndex.has_value() && primitive.materialIndex.value() >= asset.materials.size())
            {
                return false;
            }

            std::size_t indexCount = vertexCount;

            if (primitive.indicesAccessor.has_value())
            {
                const std::size_t accessorIndex = primitive.indicesAccessor.value();
                if (!validateAccessor(asset, accessorIndex, fastgltf::AccessorType::Scalar))
                {
                    return false;
                }

                const auto &accessor = asset.accessors[accessorIndex];
                if (accessor.normalized ||
                    (accessor.componentType != fastgltf::ComponentType::UnsignedByte &&
                     accessor.componentType != fastgltf::ComponentType::UnsignedShort &&
                     accessor.componentType != fastgltf::ComponentType::UnsignedInt))
                {
                    return false;
                }

                if (accessor.bufferViewIndex.has_value() && asset.bufferViews[accessor.bufferViewIndex.value()].byteStride.has_value())
                {
                    return false;
                }

                indexCount = accessor.count;
                if (indexCount > maximumIndices)
                    return false;

                bool indicesValid = true;
                fastgltf::iterateAccessor<unsigned int>(
                    asset, accessor, [&](unsigned int index) {
                        if (index >= vertexCount)
                            indicesValid = false;
                    });

                if (!indicesValid)
                    return false;
            }

            if (indexCount < 3 || indexCount > maximumIndices)
                return false;

            switch (primitive.type)
            {
            case fastgltf::PrimitiveType::Triangles:
                if (indexCount % 3 != 0)
                    return false;
                break;
            case fastgltf::PrimitiveType::TriangleStrip:
            case fastgltf::PrimitiveType::TriangleFan:
                if (indexCount - 2 > maximumIndices / 3)
                    return false;
                break;
            default:
                return false;
            }
        }
    }

    return true;
}

struct DecodedMesh
{
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<Mesh::vertexAttribute> attributes;
    unsigned int stride = 0;
    int materialIndex = -1;
};

std::optional<Model> ModelLoader::loadGltf(const std::string &path)
{
    Model model;

    // recieve raw data from file and place it into a data buffer
    const auto native = Utils::pathFromUtf8(path);
    if (!native) return std::nullopt;

    auto data = fastgltf::GltfDataBuffer::FromPath(*native);
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
    auto asset = parser.loadGltfBinary(data.get(), native->parent_path());
    if (asset.error() != fastgltf::Error::None)
    {
        Log::Print("FAILED TO PARSE GLTF " + path + " Error: " + std::string(fastgltf::getErrorMessage(asset.error())),
                   "ModelLoader",
                   LogType::LOG_ERROR);
        return std::nullopt;
    }

    fastgltf::Asset &gltf = asset.get();
    if (!validateGeometry(gltf))
    {
        Log::Print("INVALID OR UNSUPPORTED GLTF GEOMETRY: " + path,
                   "ModelLoader", LogType::LOG_ERROR);
        return std::nullopt;
    }

    std::vector<DecodedMesh> decodedMeshes;

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

            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                std::vector<unsigned int> triangles;
                triangles.reserve((indexdata.size() - 2) * 3);

                for (std::size_t i = 2; i < indexdata.size(); ++i)
                {
                    unsigned int a;
                    unsigned int b = indexdata[i - 1];
                    const unsigned int c = indexdata[i];

                    if (primitive.type == fastgltf::PrimitiveType::TriangleStrip)
                    {
                        a = indexdata[i - 2];
                        if (i % 2 != 0)
                            std::swap(a, b);
                    }
                    else
                    {
                        a = indexdata[0];
                    }

                    triangles.push_back(a);
                    triangles.push_back(b);
                    triangles.push_back(c);
                }

                indexdata = std::move(triangles);
            }

            if (mirrored)
            {
                for (std::size_t index = 0; index + 2 < indexdata.size(); index += 3)
                {
                    std::swap(indexdata[index + 1], indexdata[index + 2]);
                }
            }

            const int materialIndex = primitive.materialIndex.has_value() ? static_cast<int>(primitive.materialIndex.value()) : -1;

            decodedMeshes.push_back({
                std::move(vertexData),
                std::move(indexdata),
                std::move(attributes),
                currentOffset,
                materialIndex,
            });
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

    struct PendingNode
    {
        std::size_t index;
        fastgltf::math::fmat4x4 parentTransform;
    };

    std::vector<PendingNode> pendingNodes;
    std::vector<bool> visited(gltf.nodes.size(), false);

    const auto &roots = gltf.scenes[sceneIndex].nodeIndices;
    for (auto it = roots.rbegin(); it != roots.rend(); ++it)
    {
        pendingNodes.push_back({*it, fastgltf::math::fmat4x4()});
    }

    while (!pendingNodes.empty())
    {
        const PendingNode pending = pendingNodes.back();
        pendingNodes.pop_back();

        if (pending.index >= gltf.nodes.size() || visited[pending.index])
        {
            Log::Print("INVALID GLTF NODE HIERARCHY: " + path, "ModelLoader", LogType::LOG_ERROR);
            return std::nullopt;
        }

        visited[pending.index] = true;
        auto &node = gltf.nodes[pending.index];
        const auto nodeTransform = fastgltf::getTransformMatrix(node, pending.parentTransform);

        if (node.meshIndex.has_value())
        {
            if (node.meshIndex.value() >= gltf.meshes.size())
            {
                Log::Print("INVALID GLTF NODE MESH INDEX: " + path, "ModelLoader", LogType::LOG_ERROR);
                return std::nullopt;
            }

            glm::mat4 worldTransform(1.0f);
            for (glm::length_t column = 0; column < 4; ++column)
            {
                for (glm::length_t row = 0; row < 4; ++row)
                {
                    worldTransform[column][row] = nodeTransform[static_cast<std::size_t>(column)] [static_cast<std::size_t>(row)];
                }
            }

            appendMesh(node.meshIndex.value(), worldTransform);
        }

        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it)
        {
            pendingNodes.push_back({*it, nodeTransform});
        }
    }

    if (modelMin.x <= modelMax.x)
    {
        model.boundsMin = modelMin;
        model.boundsMax = modelMax;
    }

    if (decodedMeshes.empty())
    {
        Log::Print("GLTF CONTAINS NO USABLE MESHES: " + path, "ModelLoader", LogType::LOG_ERROR);
        model.destroy();
        return std::nullopt;
    }

        std::array<std::unordered_map<std::size_t, int>, 2> imageToTexture;

    const auto loadTexture = [&](std::size_t textureIndex, bool isSRGB) -> int {
        if (textureIndex >= gltf.textures.size())
        {
            Log::Print("INVALID GLTF TEXTURE INDEX", "ModelLoader", LogType::LOG_ERROR);
            return -1;
        }

        const auto &texture = gltf.textures[textureIndex];
        if (!texture.imageIndex.has_value())
            return -1;

        const std::size_t imageIndex = texture.imageIndex.value();
        if (imageIndex >= gltf.images.size())
        {
            Log::Print("INVALID GLTF IMAGE INDEX", "ModelLoader", LogType::LOG_ERROR);
            return -1;
        }

        auto &cache = imageToTexture[isSRGB ? 1 : 0];
        if (const auto found = cache.find(imageIndex); found != cache.end())
            return found->second;

        const auto &image = gltf.images[imageIndex];
        std::span<const std::byte> bytes;

        if (const auto *view = std::get_if<fastgltf::sources::BufferView>(&image.data))
        {
            bytes = getBufferViewBytes(gltf, view->bufferViewIndex);
        }
        else
        {
            bytes = getResidentBytes(image.data);
        }

        if (bytes.empty() ||
            bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            model.textures.size() >=
                static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            Log::Print("INVALID OR UNSUPPORTED GLTF IMAGE DATA", "ModelLoader", LogType::LOG_ERROR);
            return -1;
        }

        Texture uploaded;
        if (!uploaded.loadFromMemory(reinterpret_cast<const unsigned char *>(bytes.data()), static_cast<int>(bytes.size()), isSRGB))
        {
            return -1;
        }

        const int index = static_cast<int>(model.textures.size());
        model.textures.push_back(std::move(uploaded));
        cache.emplace(imageIndex, index);
        return index;
    };

    for (const auto &source : gltf.materials)
    {
        Material material;

        material.baseColorFactor = glm::vec4(
            source.pbrData.baseColorFactor[0],
            source.pbrData.baseColorFactor[1],
            source.pbrData.baseColorFactor[2],
            source.pbrData.baseColorFactor[3]);

        material.metallicFactor = source.pbrData.metallicFactor;
        material.roughnessFactor = source.pbrData.roughnessFactor;

        if (source.pbrData.baseColorTexture.has_value())
        {
            material.baseColorTextureIndex =
                loadTexture(source.pbrData.baseColorTexture->textureIndex, true);
        }

        if (source.pbrData.metallicRoughnessTexture.has_value())
        {
            material.metallicRoughnessTextureIndex =
                loadTexture(source.pbrData.metallicRoughnessTexture->textureIndex, false);
        }

        if (source.normalTexture.has_value())
        {
            material.normalTextureIndex =
                loadTexture(source.normalTexture->textureIndex, false);
        }

        model.materials.push_back(std::move(material));
    }

    for (const auto &decoded : decodedMeshes)
    {
        Mesh mesh;
        mesh.setup(decoded.vertices, decoded.indices, decoded.attributes, decoded.stride);
        mesh.materialIndex = decoded.materialIndex;
        model.meshes.push_back(std::move(mesh));
    }

    return model;
};
} // namespace Cthulhu::Rendering
