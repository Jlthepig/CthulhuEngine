#include "framebuffer.hpp"
#include "log_utils.hpp"

namespace Cthulhu::Rendering
{
bool Framebuffer::create(unsigned int width, unsigned int height)
{
    if (fbo != 0 || colorTexture != 0 || depthRBO != 0)
        return this->width == width && this->height == height;

    if (width == 0 || height == 0)
        return false;

    GLint maximumTextureSize{};
    GLint maximumRenderbufferSize{};
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maximumRenderbufferSize);

    const GLint maximumSize =
        std::min(maximumTextureSize, maximumRenderbufferSize);

    if (maximumSize <= 0 ||
        width > static_cast<unsigned int>(maximumSize) ||
        height > static_cast<unsigned int>(maximumSize))
    {
        return false;
    }

    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &colorTexture);
    glGenRenderbuffers(1, &depthRBO);

    if (fbo == 0 || colorTexture == 0 || depthRBO == 0)
    {
        destroy();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, colorTexture, 0);

    GLint colorWidth{};
    GLint colorHeight{};
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &colorWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &colorHeight);

    glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          static_cast<GLsizei>(width),
                          static_cast<GLsizei>(height));
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, depthRBO);

    GLint depthWidth{};
    GLint depthHeight{};
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &depthWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &depthHeight);

    const bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
        colorWidth == static_cast<GLint>(width) &&
        colorHeight == static_cast<GLint>(height) &&
        depthWidth == static_cast<GLint>(width) &&
        depthHeight == static_cast<GLint>(height);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    if (!complete)
    {
        destroy();
        return false;
    }

    this->width = width;
    this->height = height;
    return true;
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

bool Framebuffer::resize(unsigned int width, unsigned int height)
{
    if (fbo != 0 && this->width == width && this->height == height)
        return true;

    Framebuffer replacement;
    if (!replacement.create(width, height))
        return false;

    destroy();

    std::swap(fbo, replacement.fbo);
    std::swap(colorTexture, replacement.colorTexture);
    std::swap(depthRBO, replacement.depthRBO);
    std::swap(this->width, replacement.width);
    std::swap(this->height, replacement.height);

    return true;
}

void Framebuffer::blitToScreen(int x, int y, unsigned int screenWidth, unsigned int screenHeight)
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0,
                      0,
                      static_cast<GLint>(width),
                      static_cast<GLint>(height),
                      x,
                      y,
                      x + static_cast<GLint>(screenWidth),
                      y + static_cast<GLint>(screenHeight),
                      GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
}

void Framebuffer::destroy()
{
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &colorTexture);
    glDeleteRenderbuffers(1, &depthRBO);
    fbo = 0;
    colorTexture = 0;
    depthRBO = 0;
    width = 0;
    height = 0;
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
} // namespace Cthulhu::Rendering
