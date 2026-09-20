#include "engine.h"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        Log::Print(
            "USAGE: CthulhuRuntime <path-to-project.cthulhu>",
            "Runtime",
            LogType::LOG_ERROR);

        return 1;
    }

    Cthulhu::Engine engine;

    if (!engine.init(argv[1]))
    {
        Log::Print(
            "FAILED TO INITIALIZE PROJECT",
            "Runtime",
            LogType::LOG_ERROR);

        return 1;
    }

    const auto* project = engine.getProject();

    if (!project)
    {
        Log::Print(
            "ENGINE HAS NO ACTIVE PROJECT",
            "Runtime",
            LogType::LOG_ERROR);

        engine.shutdown();
        return 1;
    }

    if (!project->hasMainScene())
    {
        Log::Print(
            "PROJECT HAS NO MAIN SCENE",
            "Runtime",
            LogType::LOG_ERROR);

        engine.shutdown();
        return 1;
    }

    engine.loadScene(*project->getMainScene());

    engine.run();
    engine.shutdown();

    return 0;
}