#include "engine.hpp"
#include "log_utils.hpp"
#include "testRunner.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		Log::Print("USAGE: CthulhuTests <path-to-project.cthulhu>", "Tests", LogType::LOG_ERROR);
		return 1;
	}

	Cthulhu::Engine engine;

	if (!engine.init(argv[1]))
	{
		Log::Print("FAILED TO INITIALIZE PROJECT", "Tests", LogType::LOG_ERROR);
		return 1;
	}

	const auto* project = engine.getProject();

	if (!project || !project->hasMainScene() || !engine.loadScene(*project->getMainScene()))
	{
		Log::Print("TEST PROJECT NEEDS A LOADABLE MAIN SCENE", "Tests", LogType::LOG_ERROR);
		return 1;
	}

	// run() ends with engine shutdown as part of the shutdown tests
	return Cthulhu::Validation::run(engine) == 0 ? 0 : 1;
}
