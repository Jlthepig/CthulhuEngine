#pragma once
#include "glad.h"

namespace Cthulhu::Rendering
{
    class Framebuffer
    {
        public:
        void create(unsigned int width, unsigned int height);
        void resize(unsigned int width, unsigned int height);
        void bind();
        void unbind();
        void destroy();

        void blitToScreen(unsigned int screenWidth, unsigned int screenHeight);
        GLuint getColorTexture() const;
        unsigned int getWidth() const;
        unsigned int getHeight() const;
        private:
        GLuint fbo = 0;
        GLuint colorTexture = 0;
        GLuint depthRBO = 0;
        unsigned int width= 0, height= 0;
        
    };
}