#include <limits>

#include "history.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scribe
{
namespace
{
constexpr uint64_t UNREACHABLE = std::numeric_limits<uint64_t>::max();
}

void History::record(std::unique_ptr<Command> command)
{
    redoStack.clear();

    if (mergeOpen && !undoStack.empty() && undoStack.back().serial != savedSerial &&
        undoStack.back().command->mergeWith(*command))
    {
        return;
    }

    undoStack.push_back({std::move(command), nextSerial++});
    mergeOpen = true;

    if (undoStack.size() > maxDepth)
    {
        const uint64_t dropped = undoStack.front().serial;
        undoStack.erase(undoStack.begin());

        if (savedSerial == dropped)
        {
            savedSerial = 0;
        }
        else if (savedSerial == 0)
        {
            savedSerial = UNREACHABLE;
        }
    }
}

bool History::undo(Context &context)
{
    mergeOpen = false;

    if (undoStack.empty())
    {
        return false;
    }

    Entry entry = std::move(undoStack.back());
    undoStack.pop_back();

    entry.command->revert(context);
    redoStack.push_back(std::move(entry));
    return true;
}

bool History::redo(Context &context)
{
    mergeOpen = false;

    if (redoStack.empty())
    {
        return false;
    }

    Entry entry = std::move(redoStack.back());
    redoStack.pop_back();

    const Result result = entry.command->apply(context);
    if (!result.ok())
    {
        // only really possible if something edited the scene outside Scribe
        Log::Print("REDO FAILED: " + std::string(entry.command->name()) + ": " + result.error, "Scribe",
                   LogType::LOG_ERROR);
        redoStack.clear();
        return false;
    }

    undoStack.push_back(std::move(entry));
    return true;
}

std::string_view History::undoName() const noexcept
{
    return undoStack.empty() ? std::string_view{} : undoStack.back().command->name();
}

std::string_view History::redoName() const noexcept
{
    return redoStack.empty() ? std::string_view{} : redoStack.back().command->name();
}

void History::markSaved() noexcept
{
    mergeOpen = false;
    savedSerial = currentSerial();
}

void History::invalidateSavePoint() noexcept
{
    mergeOpen = false;
    savedSerial = UNREACHABLE;
}

void History::clear() noexcept
{
    undoStack.clear();
    redoStack.clear();
    savedSerial = 0;
    mergeOpen = false;
}
} // namespace Cthulhu::Scribe