#include <unordered_map>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "audio.hpp"
#include "log_utils.hpp"
namespace Cthulhu::Core
{

static ma_engine g_audioEngine;
static bool g_audioReady = false;

struct AudioClipData
{
    ma_sound sound;
};

std::unordered_map<uint32_t, ma_sound *> activeSounds;
uint32_t Audio::nextInstanceId = 1;

void Audio::init()
{
    ma_result result = ma_engine_init(NULL, &g_audioEngine);
    if (result != MA_SUCCESS)
    {
        KalaHeaders::KalaLog::Log::Print("Failed to initialize Miniaudio Engine", "Audio",
                                         KalaHeaders::KalaLog::LogType::LOG_ERROR);
        return;
    }

    g_audioReady = true;
    KalaHeaders::KalaLog::Log::Print("Miniaudio Engine initialized", "Audio",
                                     KalaHeaders::KalaLog::LogType::LOG_SUCCESS);
}

void Audio::shutdown()
{
    for (auto &pair : activeSounds)
    {
        ma_sound_uninit(pair.second);
        delete pair.second;
    }
    activeSounds.clear();
    ma_engine_uninit(&g_audioEngine);
    g_audioReady = false;
}

void Audio::update()
{
    for (auto it = activeSounds.begin(); it != activeSounds.end();)
    {
        ma_sound *pSound = it->second;
        if (ma_sound_at_end(pSound))
        {
            ma_sound_uninit(pSound);
            delete pSound;
            it = activeSounds.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Audio::stopSound(uint32_t instanceId)
{
    if (instanceId == 0)
        return;
    auto it = activeSounds.find(instanceId);
    if (it != activeSounds.end())
    {
        ma_sound *pSound = it->second;
        ma_sound_uninit(pSound);
        delete pSound;
        activeSounds.erase(it);
    }
}

AudioClipData *Audio::loadClip(const std::string &filePath)
{
    if (!g_audioReady)
    {
        return nullptr;
    }

    auto *clip = new AudioClipData();
    const ma_result result =
        ma_sound_init_from_file(&g_audioEngine, filePath.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, &clip->sound);

    if (result != MA_SUCCESS)
    {
        KalaHeaders::KalaLog::Log::Print("Failed to load audio clip: " + filePath, "Audio",KalaHeaders::KalaLog::LogType::LOG_ERROR);
        delete clip;
        return nullptr;
    }

    return clip;
}

void Audio::destroyClip(AudioClipData *clip)
{
    if (!clip)
    {
        return;
    }

    ma_sound_uninit(&clip->sound);
    delete clip;
}

uint32_t Audio::playClip(const AudioClipData *clip, float volume, bool loop)
{
    if (!g_audioReady || !clip)
    {
        return 0;
    }

    ma_sound *pSound = new ma_sound();
    const ma_result result = ma_sound_init_copy(&g_audioEngine, &clip->sound, 0, NULL, pSound);

    if (result != MA_SUCCESS)
    {
        KalaHeaders::KalaLog::Log::Print("Failed to play audio clip", "Audio",KalaHeaders::KalaLog::LogType::LOG_ERROR);
        delete pSound;
        return 0;
    }

    ma_sound_set_volume(pSound, volume);
    ma_sound_set_looping(pSound, loop);
    ma_sound_start(pSound);

    const uint32_t instanceId = nextInstanceId++;
    activeSounds[instanceId] = pSound;
    return instanceId;
}

size_t Audio::getActiveSoundCount()
{
    return activeSounds.size();
}

bool Audio::isSoundActive(uint32_t instanceId)
{
    return activeSounds.contains(instanceId);
}

}; // namespace Cthulhu::Core