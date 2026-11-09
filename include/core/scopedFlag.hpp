#pragma once

namespace Cthulhu::Core
{
class ScopedFlag
{
  public:
    explicit ScopedFlag(bool &flag)
        : flag(flag), previous(flag)
    {
        flag = true;
    }

    ~ScopedFlag()
    {
        flag = previous;
    }

    ScopedFlag(const ScopedFlag &) = delete;
    ScopedFlag &operator=(const ScopedFlag &) = delete;

  private:
    bool &flag;
    bool previous;
};
} // namespace Cthulhu::Core
