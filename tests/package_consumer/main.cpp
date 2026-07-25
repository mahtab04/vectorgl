#include <vectorgl/canvas.hpp>
#include <vectorgl/color.hpp>
#include <vectorgl/path.hpp>

int main()
{
    // Constructing Canvas forces the consumer to link the renderer and vendored
    // GLAD objects without requiring an OpenGL context for this smoke test.
    vectorgl::Canvas canvas;

    vectorgl::Path2D path;
    path.moveTo(0.0f, 0.0f);
    path.lineTo(20.0f, 0.0f);
    path.lineTo(20.0f, 20.0f);
    path.closePath();

    const auto color = vectorgl::Color::hex(0x3366FF);
    return path.empty() || color.a != 1.0f ? 1 : 0;
}
