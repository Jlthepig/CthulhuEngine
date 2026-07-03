#include "pch.h"
#include "audio.h"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "log_utils.hpp"

namespace Cthulhu::Core
{
    static ma_engine g_audioEngine;
    std::vector<ma_sound*> Audio::activeSounds;

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
        for (ma_sound* pSound : activeSounds)
        {
            ma_sound_uninit(pSound);
            delete pSound;
        }
        activeSounds.clear();
        ma_engine_uninit(&g_audioEngine);
    }

    void Audio::update()
    {
        for (size_t i = 0; i < activeSounds.size();)
        {
            ma_sound* pSound = activeSounds[i];
            if (ma_sound_at_end(pSound))
            {
                ma_sound_uninit(pSound);
                delete pSound;
                
                // swap and pop to avoid shifting elements
                activeSounds[i] = activeSounds.back();
                activeSounds.pop_back();
            }
            else
            {
                ++i;
            }
        }
    }

    void Audio::playSound(const char *filePath, float volume)
    {
        ma_sound* pSound = new ma_sound();
        ma_result result = ma_sound_init_from_file(&g_audioEngine,filePath, MA_SOUND_FLAG_DECODE, NULL,NULL, pSound);
        if (result != MA_SUCCESS)
        {
            KalaHeaders::KalaLog::Log::Print("Failed to play sound: " + std::string(filePath), "Audio", KalaHeaders::KalaLog::LogType::LOG_ERROR);
            delete pSound;
            return;
        }
        ma_sound_set_volume(pSound, volume);
        ma_sound_start(pSound);
        activeSounds.push_back(pSound);
    }
};