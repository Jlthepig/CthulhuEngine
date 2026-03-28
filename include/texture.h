#pragma once

#include <iostream>

namespace Cthulhu::Rendering
{
    class Texture
    {
        public:
        void load(const std::string& path);
        void bind(unsigned int slot);
        void destroy();
        unsigned int getID() const;
        unsigned int textureID = 0;
        int width = 128;
        int height = 128;
        int nrChannels;

    };
}