#include "framebuffer.h"
#include "log_utils.hpp"

namespace Cthulhu::Rendering
{
    void Framebuffer::create(unsigned int width, unsigned int height)
    {
        if (fbo != 0 || colorTexture != 0 || depthRBO != 0)
        {
            KalaHeaders::KalaLog::Log::Print("Framebuffer already created, destroy first!", "Framebuffer", KalaHeaders::KalaLog::LogType::LOG_WARNING);
            return;
        }
        this->width = width;
        this->height = height;
        if (width == 0 || height == 0)
        {
            KalaHeaders::KalaLog::Log::Print("Framebuffer dimensions cannot be zero!", "Framebuffer", KalaHeaders::KalaLog::LogType::LOG_ERROR);
            return;
        }

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &colorTexture);
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

        glGenRenderbuffers(1, &depthRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            KalaHeaders::KalaLog::Log::Print("Framebuffer not complete!", "Framebuffer", KalaHeaders::KalaLog::LogType::LOG_ERROR);
        }
        else
        {
            KalaHeaders::KalaLog::Log::Print("Framebuffer created successfully", "Framebuffer", KalaHeaders::KalaLog::LogType::LOG_SUCCESS);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

    }

    void Framebuffer::bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, width, height);
    }

    void Framebuffer::unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    void Framebuffer::resize(unsigned int width, unsigned int height)
    {
        if (width == 0 || height == 0)
        {
            KalaHeaders::KalaLog::Log::Print("Framebuffer resize dimensions cannot be zero!", "Framebuffer", KalaHeaders::KalaLog::LogType::LOG_ERROR);
            return;
        }
        if (this->width == width && this->height == height)
        {
            return;
        }
        else 
        {
            destroy();
            create(width, height);
        }
    }

    void Framebuffer::blitToScreen(unsigned int screenWidth, unsigned int screenHeight)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, width, height, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    void Framebuffer::destroy()
    {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &colorTexture);
        glDeleteRenderbuffers(1, &depthRBO);
    }

    GLuint Framebuffer::getColorTexture() const
    {
        return colorTexture;
    }

    unsigned int Framebuffer::getWidth() const
    {
        return width;
    }

    unsigned int Framebuffer::getHeight() const
    {
        return height;
    }
}