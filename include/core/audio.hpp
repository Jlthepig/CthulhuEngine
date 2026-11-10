#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace Cthulhu::Core
{

struct AudioClipData;

class Audio
{
  public:
    static bool init();
    static void update();
    static void shutdown();
    static void stopSound(uint32_t soundId);
    static bool setSoundSettings(uint32_t soundId, float volume, bool loop);
    static std::size_t stopClipSounds(const AudioClipData *clip);

    static AudioClipData *loadClip(const std::string &filePath, bool stream = false);
    static void setClipSourcePath(AudioClipData *clip, std::string path) noexcept;
    static void destroyClip(AudioClipData *clip);
    static uint32_t playClip(const AudioClipData *clip, float volume = 1.0f, bool loop = false);

    static size_t getActiveSoundCount();
    static bool isSoundActive(uint32_t soundId);

    static bool isClipStreamed(const AudioClipData *clip);

  private:
    static uint32_t nextInstanceId;
};
} // namespace Cthulhu::Core
