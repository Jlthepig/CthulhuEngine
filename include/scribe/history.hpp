#pragma once

#include "cstddef"
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "command.hpp"

namespace Cthulhu::Scribe
{
    class History
    {
        public:
            explicit History(std::size_t maxDepth = 512) : maxDepth(maxDepth)
            {
            }

            void record(std::unique_ptr<Command> command);

            bool undo(Context &context);
            bool redo(Context &context);

            [[nodiscard]] bool canUndo() const noexcept
            {
                return !undoStack.empty();
            }

            [[nodiscard]] bool canRedo() const noexcept
            {
                return !redoStack.empty();
            }

            [[nodiscard]] std::string_view undoName() const noexcept;
            [[nodiscard]] std::string_view redoName() const noexcept;

            void endMerge() noexcept
            {
                mergeOpen = false;
            }

            void markSaved() noexcept;

            void invalidateSavePoint() noexcept;

            [[nodiscard]] bool isAtSavePoint() const noexcept
            {
                return currentSerial() == savedSerial;
            }

            void clear() noexcept;

        private:
            struct Entry
            {
                std::unique_ptr<Command> command;
                uint64_t serial{};
            };

            std::vector<Entry> undoStack;
            std::vector<Entry> redoStack;

            std::size_t maxDepth;
            uint64_t nextSerial = 1;
            uint64_t savedSerial{};
            bool mergeOpen = false;

            [[nodiscard]] uint64_t currentSerial() const noexcept
            {
                return undoStack.empty() ? 0 : undoStack.back().serial;
            }
    };
} // namespace Cthulhu::Scribe