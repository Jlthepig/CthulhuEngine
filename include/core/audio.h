#pragma  once
#include <vector>

struct ma_sound;
namespace Cthulhu::Core
{
    class Audio
    {
        public:
        static void init();
        static void update();
        static void shutdown();
        static void playSound(const char* filePath, float volume = 1.0f);
        private:
        static std::vector<ma_sound*> activeSounds;
    };
}