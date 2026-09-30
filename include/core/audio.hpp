#pragma once
#include <cstdint>
#include <string>
namespace Cthulhu::Core
{
  
struct AudioClipData;

class Audio
{
  public:
    static void init();
    static void update();
    static void shutdown();
    static uint32_t playSound2D(const std::string &filePath, float volume = 1.0f, bool loop = false);
    static void stopSound(uint32_t soundId);
    
    static AudioClipData *loadClip(const std::string &filePath);
    static void destroyClip(AudioClipData *clip);
    static uint32_t playClip(const AudioClipData *clip, float volume = 1.0f, bool loop = false);

    static size_t getActiveSoundCount();
    static bool isSoundActive(uint32_t soundId);

  private:
    static uint32_t nextInstanceId;
};
} // namespace Cthulhu::Core