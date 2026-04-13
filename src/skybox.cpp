#include "skybox.h"
#include "stb_image.h"
#include "log_utils.hpp"
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_transform.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Rendering
{
    static float cubeVertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

     void Skybox::loadHDR(const std::string& path)
    {
        stbi_set_flip_vertically_on_load(true);

        int width, height, nrChannels;
        float* data = stbi_loadf(path.c_str(), &width, &height, &nrChannels, 0);

        if (!data)
        {
            Log::Print("FAILED TO LOAD HDR: " + path, "Skybox", LogType::LOG_ERROR);
            return;
        }
        
        glGenTextures(1, &hdrTexture);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);

    }
    
    void Skybox::convertToCubemap()
    {
        glGenTextures(1, &cubemapTexture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        for (unsigned int i = 0; i < 6; i++)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenFramebuffers(1,&captureFBO);
        glGenRenderbuffers(1, &captureRBO);
        glBindFramebuffer(GL_FRAMEBUFFER,captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER,captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,512,512);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,captureRBO);

        // 6 view matrices, one per face
        glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 captureViews[] = {
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.0f,-1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f,-1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,-1.0f, 0.0f), glm::vec3(0.0f, 0.0f,-1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, 0.0f, 1.0f), glm::vec3(0.0f,-1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, 0.0f,-1.0f), glm::vec3(0.0f,-1.0f, 0.0f))
        };

        equirectShader.use();
        equirectShader.setInt("equirectangularMap", 0);
        equirectShader.setMat4("projection", captureProjection);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);

        glViewport(0, 0, 512, 512);
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

        for (unsigned int i = 0; i < 6; i++)
        {
            equirectShader.setMat4("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemapTexture, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            cubeMesh.draw();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Skybox::load(const std::string& hdrPath)
    {
        if (isLoaded)
        {
            Log::Print("SKYBOX ALREADY LOADED", "Skybox", LogType::LOG_WARNING);
            return;
        }

        // build cube mesh
        std::vector<float> verts(cubeVertices, cubeVertices + sizeof(cubeVertices) / sizeof(float));
        std::vector<unsigned int> indices;

        for (unsigned int i = 0; i < 36; i++) indices.push_back(i);

        std::vector<Mesh::vertexAttribute> attrs = {
            {0, 3, 0}
        };

        cubeMesh.setup(verts, indices, attrs, 3 * sizeof(float));

        equirectShader.load("shaders/equirect.vertex", "shaders/equirect.fragment");
        skyboxShader.load("shaders/skybox.vertex", "shaders/skybox.fragment");

        loadHDR(hdrPath);
        convertToCubemap();

        isLoaded = true;
    }

     void Skybox::draw(GLFWwindow* window, const glm::mat4& view, const glm::mat4& projection)
    {
        // restore viewport to window size before drawing
         int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        skyboxShader.setInt("environmentMap", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);

        cubeMesh.draw();
        glDepthFunc(GL_LESS);
    }

    void Skybox::destroy()
    {
        if (!isLoaded) return;
        glDeleteTextures(1, &hdrTexture);
        glDeleteTextures(1, &cubemapTexture);
        glDeleteFramebuffers(1, &captureFBO);
        glDeleteRenderbuffers(1, &captureRBO);
        cubeMesh.destroy();
        equirectShader.destroy();
        skyboxShader.destroy();
        isLoaded = false;
    }

    

}