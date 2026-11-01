#pragma once

namespace Cthulhu::Rendering
{
class GridLines
{
  public:
    void setupGrid();
    void draw();
    void destroy();

  private:
    unsigned int VAO{};
};

} // namespace Cthulhu::Rendering
