#pragma once
#include "fwd.hpp"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "trigonometric.hpp"

namespace Cthulhu::Editor
{
    class EditorCamera
    {   
        public:
        EditorCamera(float fov, float aspectRatio,float nearPlane,float farPlane);

        // one per frame process input and update matrices
        void update(float deltaTime);

        const glm::mat4& getViewMatrix() const;
        const glm::mat4& getProjectionMatrix() const;
        glm::vec3 getPosition() const;
        glm::vec3 getFocalPoint() const;

        // editor ui helpers
        void setFocalPoint(const glm::vec3& point);
        void setAspectRatio(float aspectRatio);
        private:
        void updateCameraVectors();

        // orbiting 
        glm::vec3 focalPoint = glm::vec3(0.0f,0.0f,0.0f);
        float distance = 10.0f;
        float yaw = glm::radians(45.0f);
        float pitch = glm::radians(-30.0f);

        // settings - later move into editor config
        float orbitSpeed = 0.005f;
        float panSpeed = 0.01f;
        float zoomSpeed = 2.0f;
        float minDistance = 0.5f;
        float maxDistance = 500.0f;

        // cache
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::vec3 position;
        glm::vec3 rightVector;
        glm::vec3 upVector;
        glm::vec3 frontVector;

        float fov, aspectRatio, nearPlane, farPlane;
    };
}