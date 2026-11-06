#include <glad.h>
#include <stb_image.h>
#include <utility>

#include "log_utils.hpp"
#include "texture.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Rendering
{
Texture::~Texture()
{
    destroy();
}

Texture::Texture(Texture &&other) noexcept
{
    *this = std::move(other);
}

Texture &Texture::operator=(Texture &&other) noexcept
{
    if (this == &other)
        return *this;

    destroy();

    textureID = std::exchange(other.textureID, 0);
    width = other.width;
    height = other.height;
    nrChannels = other.nrChannels;
    isLoaded = std::exchange(other.isLoaded, false);

    return *this;
}

void Texture::load(const std::string &path)
{
    if (isLoaded)
    {
        Log::Print("TEXTURE IS ALREADY LOADED, DESTROY BEFORE RELOADING" + path, "Texture(path)", LogType::LOG_WARNING);
        return;
    }

    const char *rawPath = path.c_str();
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(rawPath, &width, &height, &nrChannels, STBI_rgb_alpha);

    if (!data)
    {
        Log::Print("IMAGE FAILED TO LOAD: " + path, "Image", LogType::LOG_ERROR);
        return;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR); // Use mipmaps if generated
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    isLoaded = true;
}

bool Texture::loadFromMemory(const unsigned char *data, int length, bool isSRGB)
{
    if (isLoaded || data == nullptr || length <= 0)
        return false;

    stbi_set_flip_vertically_on_load(false);

    unsigned char *pixels = stbi_load_from_memory(data, length, &width, &height,&nrChannels, STBI_rgb_alpha);

    if (!pixels)
    {
        Log::Print("FAILED TO LOAD IMAGE FROM MEMORY", "Texture", LogType::LOG_ERROR);
        return false;
    }

    GLint maximumSize{};
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumSize);

    if (width <= 0 || height <= 0 ||
        width > maximumSize || height > maximumSize)
    {
        stbi_image_free(pixels);
        Log::Print("IMAGE EXCEEDS GPU TEXTURE LIMITS", "Texture", LogType::LOG_ERROR);
        return false;
    }

    glGenTextures(1, &textureID);
    if (textureID == 0)
    {
        stbi_image_free(pixels);
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    const GLenum internalFormat = isSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    stbi_image_free(pixels);

    GLint allocatedWidth{};
    GLint allocatedHeight{};
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &allocatedWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &allocatedHeight);

    if (allocatedWidth != width || allocatedHeight != height)
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        destroy();
        Log::Print("FAILED TO ALLOCATE GPU TEXTURE", "Texture", LogType::LOG_ERROR);
        return false;
    }

    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    isLoaded = true;
    return true;
}

void Texture::bind(unsigned int slot)
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void Texture::destroy()
{
    if (textureID != 0) glDeleteTextures(1, &textureID);

    textureID = 0;
    isLoaded = false;
}

unsigned int Texture::getID() const
{
    return textureID;
}
} // namespace Cthulhu::Rendering
