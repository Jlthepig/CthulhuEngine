#include "shader.h"
#include "log_utils.hpp"


using KalaHeaders::KalaLog::Log; 
using KalaHeaders::KalaLog::LogType; 

namespace Cthulhu::Rendering
{
    void Shader::load(const std::string& vertexPath, const std::string& fragmentPath)
    {
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
    }
    
     void Shader::use()
    {
        glUseProgram(shaderProgram);
    }
    
    void Shader::setInt(const std::string& name,int value)
    {

        const char* NAME = name.c_str();
        glUniform1i(glGetUniformLocation(shaderProgram, NAME), value);
    }
    
    void Shader::setMat4(const std::string& name, const glm::mat4& matrix)
    {
        const char* NAME = name.c_str();
        glUniformMatrix4fv( glGetUniformLocation(shaderProgram, NAME), 1, GL_FALSE, &matrix[0][0]);
    }
    
    void Shader::destroy()
    {
        glDeleteProgram(shaderProgram);
    }
    
    unsigned int Shader::getId() const
    {
       return shaderProgram;
    }

    
}