#pragma once
#include "fileReader.h"

#include "fwd.hpp"
#include "glm.hpp"
#include "glad.h"
#include "glfw3.h"


namespace Cthulhu::Rendering
{

    class Shader
    {
        public:
        void load(const std::string& vertexPath, const std::string& fragmentPath);
        void use();
        void setInt(const std::string& name,int value);
        void setMat4(const std::string& name, const glm::mat4& matrix);
        void destroy();
        unsigned int getId() const;
        private:
        unsigned int shaderProgram = 0;

    };
}