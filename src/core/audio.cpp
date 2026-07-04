#include "pch.h"
#include "audio.h"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "log_utils.hpp"
#include <unordered_map>
namespace Cthulhu::Core
{
    static ma_engine g_audioEngine;
    std::unordered_map<uint32_t, ma_sound*> activeSounds;
    uint32_t Audio::nextInstanceId = 1;

    void Audio::init()
    {
        ma_result result = ma_engine_init(NULL, &g_audioEngine);
        if (result != MA_SUCCESS)
        {
            KalaHeaders::KalaLog::Log::Print("Failed to initialize Miniaudio Engine", "Audio", KalaHeaders::KalaLog::LogType::LOG_ERROR);
            return;
        }
        KalaHeaders::KalaLog::Log::Print("Miniaudio Engine initialized", "Audio", KalaHeaders::KalaLog::LogType::LOG_SUCCESS);
    }

    void Audio::shutdown()
    {
        for (auto& pair : activeSounds)
        {
            ma_sound_uninit(pair.second);
            delete pair.second;
        }
        activeSounds.clear();
        ma_engine_uninit(&g_audioEngine);
    }

    void Audio::update()
    {
        for (auto it = activeSounds.begin(); it != activeSounds.end();)
        {
            ma_sound* pSound = it->second;
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

    uint32_t Audio::playSound2D(const std::string& filePath, float volume, bool loop)
    {
        ma_sound* pSound = new ma_sound();
        ma_result result = ma_sound_init_from_file(&g_audioEngine, filePath.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, pSound);
        if (result != MA_SUCCESS)
        {
            KalaHeaders::KalaLog::Log::Print("Failed to play sound: " + std::string(filePath), "Audio", KalaHeaders::KalaLog::LogType::LOG_ERROR);
            delete pSound;
            return 0;
        }
        ma_sound_set_volume(pSound, volume);
        ma_sound_set_looping(pSound, loop);
        ma_sound_start(pSound);
        uint32_t instanceId = nextInstanceId++;
        activeSounds[instanceId] = pSound;
        return instanceId;
    }

    void Audio::stopSound(uint32_t instanceId)
    {
        if (instanceId == 0) return;
        auto it = activeSounds.find(instanceId);
        if (it != activeSounds.end())
        {
            ma_sound * pSound = it->second; 
            ma_sound_uninit(pSound);
            delete pSound;
            activeSounds.erase(it);
        }
    }
};