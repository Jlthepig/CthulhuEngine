#include <unordered_map>
#include <cmath>

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
    std::string filePath;
    bool stream = false;
    bool hasTemplate = false;
    ma_sound sound;
};

struct AudioVoice
{
    ma_sound *sound = nullptr;
    const AudioClipData *clip = nullptr;
};

using VoiceMap = std::unordered_map<uint32_t, AudioVoice>;

static VoiceMap activeSounds;

uint32_t Audio::nextInstanceId = 1;
static VoiceMap::iterator destroyVoice(VoiceMap::iterator voice)
{
    ma_sound_uninit(voice->second.sound);
    delete voice->second.sound;
    return activeSounds.erase(voice);
}

bool Audio::init()
{
    if (g_audioReady)
    {
        return true;
    }

    const ma_result result = ma_engine_init(nullptr, &g_audioEngine);
    if (result != MA_SUCCESS)
    {
        KalaHeaders::KalaLog::Log::Print("Failed to initialize Miniaudio Engine", "Audio",KalaHeaders::KalaLog::LogType::LOG_ERROR);
        return false;
    }

    g_audioReady = true;
    KalaHeaders::KalaLog::Log::Print("Miniaudio Engine initialized", "Audio",KalaHeaders::KalaLog::LogType::LOG_SUCCESS);
    return true;
}

void Audio::shutdown()
{
    if (!g_audioReady)
        return;

    while (!activeSounds.empty())
    {
        destroyVoice(activeSounds.begin());
    }

    ma_engine_uninit(&g_audioEngine);
    g_audioReady = false;
}

void Audio::update()
{
    for (auto it = activeSounds.begin(); it != activeSounds.end();)
    {
        if (ma_sound_at_end(it->second.sound))
        {
            it = destroyVoice(it);
        }
        else
        {
            ++it;
        }
    }
}

void Audio::stopSound(uint32_t instanceId)
{
    const auto voice = activeSounds.find(instanceId);
    if (voice != activeSounds.end())
    {
        destroyVoice(voice);
    }
}

bool Audio::setSoundSettings(uint32_t instanceId, float volume, bool loop)
{
    if (!std::isfinite(volume) || volume < 0.0f)
        return false;

    const auto voice = activeSounds.find(instanceId);
    if (voice == activeSounds.end())
        return false;

    ma_sound_set_volume(voice->second.sound, volume);
    ma_sound_set_looping(voice->second.sound, loop);
    return true;
}

std::size_t Audio::stopClipSounds(const AudioClipData *clip)
{
    if (!clip)
        return 0;

    std::size_t stopped{};

    for (auto it = activeSounds.begin(); it != activeSounds.end();)
    {
        if (it->second.clip == clip)
        {
            it = destroyVoice(it);
            ++stopped;
        }
        else
        {
            ++it;
        }
    }

    return stopped;
}

AudioClipData *Audio::loadClip(const std::string &filePath, bool stream)
{
    if (!g_audioReady)
    {
        return nullptr;
    }

    auto *clip = new AudioClipData();
    clip->filePath = filePath;
    clip->stream = stream;

    if (stream)
    {
        ma_sound probe;
        if (ma_sound_init_from_file(&g_audioEngine, filePath.c_str(), MA_SOUND_FLAG_STREAM, NULL, NULL, &probe) !=
            MA_SUCCESS)
        {
            KalaHeaders::KalaLog::Log::Print("Failed to open audio stream: " + filePath, "Audio", KalaHeaders::KalaLog::LogType::LOG_ERROR);
            delete clip;
            return nullptr;
        }
        ma_sound_uninit(&probe);
        return clip;
    }

    const ma_result result =
        ma_sound_init_from_file(&g_audioEngine, filePath.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, &clip->sound);

    if (result != MA_SUCCESS)
    {
        KalaHeaders::KalaLog::Log::Print("Failed to load audio clip: " + filePath, "Audio", KalaHeaders::KalaLog::LogType::LOG_ERROR);
        delete clip;
        return nullptr;
    }

    clip->hasTemplate = true;
    return clip;
}

void Audio::destroyClip(AudioClipData *clip)
{
    if (!clip)
        return;

    stopClipSounds(clip);

    if (clip->hasTemplate)
    {
        ma_sound_uninit(&clip->sound);
    }

    delete clip;
}

uint32_t Audio::playClip(const AudioClipData *clip, float volume, bool loop)
{
    if (!g_audioReady || !clip || !std::isfinite(volume) || volume < 0.0f || nextInstanceId == 0)
    {
        return 0;
    }

    ma_sound *sound = new ma_sound();

    const ma_result initResult =
        clip->stream
            ? ma_sound_init_from_file(
                  &g_audioEngine, clip->filePath.c_str(),
                  MA_SOUND_FLAG_STREAM, nullptr, nullptr, sound)
            : ma_sound_init_copy(&g_audioEngine, &clip->sound, 0, nullptr, sound);

    if (initResult != MA_SUCCESS)
    {
        KalaHeaders::KalaLog::Log::Print("Failed to initialize audio voice","Audio", KalaHeaders::KalaLog::LogType::LOG_ERROR);

        delete sound;
        return 0;
    }

    ma_sound_set_volume(sound, volume);
    ma_sound_set_looping(sound, loop);

    const ma_result startResult = ma_sound_start(sound);
    if (startResult != MA_SUCCESS)
    {
        ma_sound_uninit(sound);
        delete sound;

        KalaHeaders::KalaLog::Log::Print("Failed to start audio voice", "Audio", KalaHeaders::KalaLog::LogType::LOG_ERROR);

        return 0;
    }

    const uint32_t instanceId = nextInstanceId++;
    activeSounds.emplace(instanceId, AudioVoice{sound, clip});
    return instanceId;
}

bool Audio::isClipStreamed(const AudioClipData *clip)
{
    return clip && clip->stream;
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
