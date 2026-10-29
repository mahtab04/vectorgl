#pragma once
#include <cmath>
#include <memory>
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

/*! @brief Determines which regions of a compound path are filled. */
enum class FillRule
{
    NonZero, //!< Fill regions with nonzero signed winding (default).
    EvenOdd  //!< Fill regions crossed an odd number of times, regardless of direction.
};

enum class LineJoin
{
    Miter,
    Round,
    Bevel
};
enum class LineCap
{
    Butt,
    Round,
    Square
};

/*! @brief Styling for path strokes in local coordinates. */
struct StrokeStyle
{
    LineJoin join = LineJoin::Miter;
    LineCap cap = LineCap::Butt;
    float miterLimit = 10.0f; //!< Maximum join center-to-tip distance / half-width; bevel fallback.
    std::vector<float>
        dashArray{};         //!< Alternating local dash/gap lengths; odd arrays repeat twice. Empty/all-zero is solid.
    float dashOffset = 0.0f; //!< Distance into the repeating pattern; signed, finite, local units.
    bool operator==(const StrokeStyle&) const = default;
};

class Path2D;
namespace detail
{
struct PathCache;
struct PathMesh;
const std::vector<std::vector<Vec2>>& pathContours(const Path2D& path);
const PathMesh& pathMesh(const Path2D& path, FillRule rule, bool* cacheHit = nullptr);
const PathMesh& strokeMesh(const Path2D& path, float width, const StrokeStyle& style, bool* cacheHit = nullptr);
} // namespace detail

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
 *  Contours, fill meshes and the last stroke mesh are cached lazily and discarded on edits. Copies
 *  share cached geometry until edited. Concurrent access to a shared path or
 *  copies sharing a cache requires external synchronization.
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

    /*! @brief Tests the filled area in local coordinates, including its boundary.
     *  Open contours are implicitly closed. Nonfinite coordinates and coordinates
     *  outside +/-8388608 return false. No OpenGL context is needed.
     */
    bool contains(float x, float y, FillRule rule = FillRule::NonZero) const;

    /*! @brief Tests the stroked mesh in local coordinates, including its boundary.
     *  Ignores the antialias fringe. Nonpositive widths return false.
     *  Invalid coordinates/styles return false. Uses the same mesh as rendering.
     *  Overly dense dash patterns (more than 65536 interval steps) return false.
     */
    bool strokeContains(float x, float y, float width, const StrokeStyle& style = {}) const;

private:
    friend const std::vector<std::vector<Vec2>>& detail::pathContours(const Path2D& path);
    friend const detail::PathMesh& detail::pathMesh(const Path2D& path, FillRule rule, bool* cacheHit);
    friend const detail::PathMesh& detail::strokeMesh(const Path2D& path, float width, const StrokeStyle& style,
                                                      bool* cacheHit);
    std::vector<std::vector<Vec2>> buildSubPaths() const;
    std::vector<PathPoint> points_;
    Vec2 cursor_{0, 0};
    // Copies may share derived geometry; edits discard only this path's cache.
    mutable std::shared_ptr<detail::PathCache> cache_;
};

} // namespace vectorgl
