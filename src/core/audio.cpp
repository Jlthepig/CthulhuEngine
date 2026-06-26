#include "pch.h"
#include "audio.h"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "log_utils.hpp"

namespace Cthulhu::Core
{
    static ma_engine g_audioEngine;

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
        ma_engine_uninit(&g_audioEngine);
    }

    void Audio::playSound(const char *filePath)
    {
        ma_engine_play_sound(&g_audioEngine, filePath, NULL);
    }
};