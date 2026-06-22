#pragma once
#include "glm.hpp"

namespace Cthulhu::Scene 
{
    struct AABB 
    {
        glm::vec3 min;
        glm::vec3 max;
    };
    struct Plane 
    {
        glm::vec3 normal;
        float distance;
        
        float getDistanceTo(const glm::vec3& point) const {
            return glm::dot(normal, point) + distance;
        }
    };

    class Frustum 
    {
    public:
        Plane planes[6];

        // Extracts the 6 frustum planes from the combined View Projection matrix
        void extractFromMatrix(const glm::mat4& vp) 
        {
            // Left
            planes[0].normal.x = vp[0][3] + vp[0][0];
            planes[0].normal.y = vp[1][3] + vp[1][0];
            planes[0].normal.z = vp[2][3] + vp[2][0];
            planes[0].distance = vp[3][3] + vp[3][0];

            // Right
            planes[1].normal.x = vp[0][3] - vp[0][0];
            planes[1].normal.y = vp[1][3] - vp[1][0];
            planes[1].normal.z = vp[2][3] - vp[2][0];
            planes[1].distance = vp[3][3] - vp[3][0];

            // Bottom
            planes[2].normal.x = vp[0][3] + vp[0][1];
            planes[2].normal.y = vp[1][3] + vp[1][1];
            planes[2].normal.z = vp[2][3] + vp[2][1];
            planes[2].distance = vp[3][3] + vp[3][1];

            // Top
            planes[3].normal.x = vp[0][3] - vp[0][1];
            planes[3].normal.y = vp[1][3] - vp[1][1];
            planes[3].normal.z = vp[2][3] - vp[2][1];
            planes[3].distance = vp[3][3] - vp[3][1];

            // Near
            planes[4].normal.x = vp[0][3] + vp[0][2];
            planes[4].normal.y = vp[1][3] + vp[1][2];
            planes[4].normal.z = vp[2][3] + vp[2][2];
            planes[4].distance = vp[3][3] + vp[3][2];

            // Far
            planes[5].normal.x = vp[0][3] - vp[0][2];
            planes[5].normal.y = vp[1][3] - vp[1][2];
            planes[5].normal.z = vp[2][3] - vp[2][2];
            planes[5].distance = vp[3][3] - vp[3][2];

            // Normalize the planes (CRITICAL for accurate distance checks)
            for (int i = 0; i < 6; i++) 
            {
                float length = glm::length(planes[i].normal);
                planes[i].normal /= length;
                planes[i].distance /= length;
            }
        }

        // The p-vertex test: The fastest, most accurate way to test an AABB against planes
        bool testAABB(const glm::vec3& min, const glm::vec3& max) const 
        {
            for (int i = 0; i < 6; i++) 
            {
                // Find the "positive vertex" (the corner of the box furthest along the plane's normal)
                glm::vec3 pVertex;
                pVertex.x = (planes[i].normal.x >= 0.0f) ? max.x : min.x;
                pVertex.y = (planes[i].normal.y >= 0.0f) ? max.y : min.y;
                pVertex.z = (planes[i].normal.z >= 0.0f) ? max.z : min.z;

                // If the furthest corner is behind the plane, the ENTIRE box is outside
                if (planes[i].getDistanceTo(pVertex) < 0.0f) 
                {
                    return false; 
                }
            }
            return true; // If it passed all 6 planes, it's visible!
        }
    };
}