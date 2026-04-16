#pragma once

#include "glm.hpp"
#include "entity.h"



namespace Cthulhu::Rendering
{
    struct Frustum
    {
        glm::vec4 planes[6];

        void extractFromMatrix(glm::mat4 viewProjection)
        {
            // Left plane
            planes[0] = glm::vec4(
                viewProjection[0][3] + viewProjection[0][0],
                viewProjection[1][3] + viewProjection[1][0],
                viewProjection[2][3] + viewProjection[2][0],
                viewProjection[3][3] + viewProjection[3][0]
            );

            // Right plane
            planes[1] = glm::vec4(
                viewProjection[0][3] - viewProjection[0][0],
                viewProjection[1][3] - viewProjection[1][0],
                viewProjection[2][3] - viewProjection[2][0],
                viewProjection[3][3] - viewProjection[3][0]
            );

            // Bottom plane
            planes[2] = glm::vec4(
                viewProjection[0][3] + viewProjection[0][1],
                viewProjection[1][3] + viewProjection[1][1],
                viewProjection[2][3] + viewProjection[2][1],
                viewProjection[3][3] + viewProjection[3][1]
            );

            // Top plane
            planes[3] = glm::vec4(
                viewProjection[0][3] - viewProjection[0][1],
                viewProjection[1][3] - viewProjection[1][1],
                viewProjection[2][3] - viewProjection[2][1],
                viewProjection[3][3] - viewProjection[3][1]
            );

            // Near plane
            planes[4] = glm::vec4(
                viewProjection[0][3] + viewProjection[0][2],
                viewProjection[1][3] + viewProjection[1][2],
                viewProjection[2][3] + viewProjection[2][2],
                viewProjection[3][3] + viewProjection[3][2]
            );

            // Far plane
            planes[5] = glm::vec4(
                viewProjection[0][3] - viewProjection[0][2],
                viewProjection[1][3] - viewProjection[1][2],
                viewProjection[2][3] - viewProjection[2][2],
                viewProjection[3][3] - viewProjection[3][2]
            );

            // Normalize the planes
            for (int i = 0; i < 6; i++)
            {
                float length = glm::length(glm::vec3(planes[i]));
                planes[i] /= length;
            }
        }

        bool testAABB(Scene::AABB bounds)
        {
            for (int i = 0; i<6; i++)
            {
                glm::vec3 planeNormal = glm::vec3(planes[i]);
                float planeDistance = planes[i].w;
                glm::vec3 positiveVertex = bounds.min;

                // Find the positive vertex for the current plane
                if (planeNormal.x > 0) positiveVertex.x = bounds.max.x; else positiveVertex.x = bounds.min.x;
                if (planeNormal.y > 0) positiveVertex.y = bounds.max.y; else positiveVertex.y = bounds.min.y;
                if (planeNormal.z > 0) positiveVertex.z = bounds.max.z; else positiveVertex.z = bounds.min.z;

                // Calculate the distance from the positive vertex to the plane
                float distance = glm::dot(planeNormal, positiveVertex) + planeDistance;

                // If the distance is negative, the AABB is completely outside the frustum
                if (distance < 0)
                    return false;
            }
            return true;
        }

        
    };
}