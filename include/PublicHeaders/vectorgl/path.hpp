#pragma once
#include <cmath>
#include <numbers>
#include <vector>

namespace vectorgl
{

/*! @brief A 2D point / vector with basic arithmetic.
 */
struct Vec2
{
    float x = 0.0f, y = 0.0f;
    Vec2 operator+(Vec2 o) const
    {
        return {x + o.x, y + o.y};
    }
    Vec2 operator-(Vec2 o) const
    {
        return {x - o.x, y - o.y};
    }
    Vec2 operator*(float s) const
    {
        return {x * s, y * s};
    }
    float length() const
    {
        return std::sqrt(x * x + y * y);
    }
    Vec2 normalized() const
    {
        float l = length();
        return l > 0 ? Vec2{x / l, y / l} : Vec2{0, 0};
    }
    Vec2 perp() const
    {
        return {-y, x};
    }
};

enum class PathCmd
{
    MoveTo,
    LineTo,
    Close
};

struct PathPoint
{
    PathCmd cmd;
    Vec2 pos;
};

/*! @brief A 2D path builder following the HTML5 Canvas path API.
 *
 *  Build complex shapes from lines, cubic/quadratic Bézier curves, arcs,
 *  and ellipses.  The resulting path can be filled or stroked via Canvas.
 *
 *  @code
 *  vectorgl::Path2D path;
 *  path.moveTo(10, 10);
 *  path.lineTo(100, 10);
 *  path.bezierCurveTo(120, 30, 120, 70, 100, 90);
 *  path.closePath();
 *  @endcode
 */
class Path2D
{
public:
    /*! @brief Starts a new sub-path at the given point.
     *  @param[in] x  Start X.  @param[in] y  Start Y.
     */
    void moveTo(float x, float y);

    /*! @brief Adds a straight line from the current point.
     *  @param[in] x  End X.  @param[in] y  End Y.
     */
    void lineTo(float x, float y);

    /*! @brief Adds a cubic Bézier curve from the current point.
     *  @param[in] cp1x,cp1y  First control point.
     *  @param[in] cp2x,cp2y  Second control point.
     *  @param[in] x,y        End point.
     */
    void bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);

    /*! @brief Adds a quadratic Bézier curve from the current point.
     *  @param[in] cpx,cpy  Control point.
     *  @param[in] x,y      End point.
     */
    void quadraticCurveTo(float cpx, float cpy, float x, float y);

    /*! @brief Adds a circular arc.
     *  @param[in] cx          Center X.
     *  @param[in] cy          Center Y.
     *  @param[in] r           Radius.
     *  @param[in] startAngle  Start angle in radians.
     *  @param[in] endAngle    End angle in radians.
     *  @param[in] ccw         Counter-clockwise if `true`.
     */
    void arc(float cx, float cy, float r, float startAngle, float endAngle, bool ccw = false);

    /*! @brief Adds an elliptical arc.
     *  @param[in] cx          Center X.       @param[in] cy          Center Y.
     *  @param[in] rx          Horizontal radius. @param[in] ry       Vertical radius.
     *  @param[in] rotation    Ellipse rotation in radians.
     *  @param[in] startAngle  Start angle.    @param[in] endAngle    End angle.
     *  @param[in] ccw         Counter-clockwise if `true`.
     */
    void ellipse(float cx, float cy, float rx, float ry, float rotation, float startAngle, float endAngle,
                 bool ccw = false);

    /*! @brief Adds an axis-aligned rectangle as a closed sub-path.
     *  @param[in] x  Left.  @param[in] y  Top.
     *  @param[in] w  Width. @param[in] h  Height.
     */
    void rect(float x, float y, float w, float h);

    /*! @brief Closes the current sub-path with a straight line to its start. */
    void closePath();

    /*! @brief Removes all sub-paths, resetting the path to empty. */
    void clear();

    const std::vector<PathPoint>& points() const
    {
        return points_;
    }
    bool empty() const
    {
        return points_.empty();
    }

    std::vector<std::vector<Vec2>> getSubPaths() const;

private:
    std::vector<PathPoint> points_;
    Vec2 cursor_{0, 0};
};

} // namespace vectorgl
