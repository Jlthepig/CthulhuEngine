#pragma once
#include "glm.hpp"
#include <cstdint>

#define a4 alignas(4)
#define a16 alignas(16)

namespace Cthulhu::Rendering
{
    struct PointLightUBO
    {
        a16 glm::vec3 position;
        a16 glm::vec3 color;
        a4 float intensity;
        a4 float constant;
        a4 float linear;
        a4 float quadratic;
    };

    struct SceneUniforms
    {
        // camera
        a16 glm::mat4 view;
        a16 glm::mat4 projection;
        a16 glm::vec3 viewPos;

        // Sun and Fog
        a16 glm::vec3 lightDir;
        a4 float lightIntensity;
        a4 float fogDensity;
        a4 float fogHeightFalloff;
        a4 int32_t pointLightCount;

        a16 glm::vec3 lightColor;
        a16 glm::vec3 fogColor;
        a4 int32_t   pointShadowCount;
        a4 float     pointShadowfarPlane;
        a4 int32_t  pad1;
        a4 int32_t  pad2;

        PointLightUBO pointLights[8];
    };
}