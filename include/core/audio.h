#pragma  once
#include <cstdint>
#include <string>
namespace Cthulhu::Core
{
    class Audio
    {
        public:
        static void init();
        static void update();
        static void shutdown();
        static uint32_t playSound2D(const std::string& filePath, float volume = 1.0f, bool loop = false);
        static void stopSound(uint32_t soundId);
        private:
        static uint32_t nextInstanceId;
    };
}