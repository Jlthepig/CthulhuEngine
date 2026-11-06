#pragma once

#include <string>

namespace Cthulhu::Rendering
{
class Texture
{
  public:
    Texture() = default;
    ~Texture();

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;

    Texture(Texture &&other) noexcept;
    Texture &operator=(Texture &&other) noexcept;

    void load(const std::string &path);
    bool loadFromMemory(const unsigned char *data, int length, bool isSRGB = true);
    void bind(unsigned int slot);
    void destroy();
    unsigned int getID() const;

  private:
    unsigned int textureID{};
    int width = 128;
    int height = 128;
    int nrChannels{};
    bool isLoaded = false;
};
} // namespace Cthulhu::Rendering
