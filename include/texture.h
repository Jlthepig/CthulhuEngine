#pragma once

#include <iostream>

namespace Cthulhu::Rendering
{
    class Texture
    {
        public:
        void load(const std::string& path);
        void loadFromMemory(const unsigned char* data, int length);
        void bind(unsigned int slot);
        void destroy();
        unsigned int getID() const;
        

        private:
        unsigned int textureID = 0;
        int width = 128;
        int height = 128;
        int nrChannels;
        bool isLoaded = false;
    };
}