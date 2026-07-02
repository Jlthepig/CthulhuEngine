#include "pch.h"
#include "shader.h"
#include "log_utils.hpp"


using KalaHeaders::KalaLog::Log; 
using KalaHeaders::KalaLog::LogType; 

namespace Cthulhu::Rendering
{
    void Shader::load(const std::string& vertexPath, const std::string& fragmentPath)
    {   
        if (isLoaded)
        {
            Log::Print("SHADER IS ALREADY LOADED,DESTROY BEFORE RELOADING" + vertexPath + fragmentPath, "Shader", LogType::LOG_WARNING);
        return;
        }

        uniformCache.clear();
        
        std::string vertexShaderSource = Utils::FileReader::readFile(vertexPath);
        std::string fragmentShaderSource = Utils::FileReader::readFile(fragmentPath);

        unsigned int vertexShader;
        unsigned int fragmentShader;

        const char* rawVertex = vertexShaderSource.c_str();
        const char* rawFragment = fragmentShaderSource.c_str();

        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader,1,&rawVertex,NULL);
        glCompileShader(vertexShader);

        GLint success;
        GLchar infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            Log::Print("SHADER COMPILE FAILED","Shader",LogType::LOG_ERROR);
            glGetShaderInfoLog(vertexShader,512,NULL,infoLog);
            printf("%s", infoLog);
        }

        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader,1,&rawFragment,NULL);
        glCompileShader(fragmentShader);

        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            Log::Print("SHADER COMPILE FAILED","Shader",LogType::LOG_ERROR);
            glGetShaderInfoLog(fragmentShader,512,NULL,infoLog);
            printf("%s", infoLog);
        }

        shaderProgram = glCreateProgram();

        glAttachShader(shaderProgram,vertexShader);
        glAttachShader(shaderProgram,fragmentShader);
        glLinkProgram(shaderProgram);

        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            Log::Print(" SHADER PROGRAM LINKING FAILED","ShaderProgram",LogType::LOG_ERROR);
           glGetProgramInfoLog(shaderProgram,512,NULL,infoLog);
           printf("%s", infoLog);
        }
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        isLoaded = true;
    }
    
     void Shader::use()
    {
        glUseProgram(shaderProgram);
    }

    GLint Shader::getUniformLocation(const std::string& name)
    {
        auto it = uniformCache.find(name);
        if (it != uniformCache.end()) return it->second;
        GLint loc = glGetUniformLocation(shaderProgram, name.c_str());
        uniformCache[name] = loc;
        return loc;
    }
    
    void Shader::setInt(const std::string& name,int value)
    {
        glUniform1i(getUniformLocation(name), value);
    }
    
    void Shader::setMat4(const std::string& name, const glm::mat4& matrix)
    {
        glUniformMatrix4fv( getUniformLocation(name), 1, GL_FALSE, &matrix[0][0]);
    }
    
    void Shader::setVec3(const std::string& name, const glm::vec3& value)
    {
        glUniform3fv(getUniformLocation(name), 1, &value[0]);
    }

    void Shader::setVec4(const std::string& name, const glm::vec4& value)
    {
        glUniform4fv(getUniformLocation(name), 1, &value[0]);
    }
    
    void Shader::setFloat(const std::string& name, float value)
    {
        glUniform1f(getUniformLocation(name), value);
    }

    void Shader::destroy()
    {   
        if (!isLoaded) return;
        glDeleteProgram(shaderProgram);
        isLoaded = false;
        uniformCache.clear();
    }
    
    unsigned int Shader::getId() const
    {
       return shaderProgram;
    }

    
}