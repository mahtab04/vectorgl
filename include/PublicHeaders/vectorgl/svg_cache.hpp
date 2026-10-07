#pragma once
#include <memory>
#include <string>

namespace vectorgl
{

class Canvas;

/*! @brief Handle-based SVG asset manager for efficient multi-SVG rendering.
 *
 *  SvgCache stores all parsed SVG data internally and returns lightweight
 *  integer handles to the caller — similar to how NanoVG manages images.
 *  This avoids per-asset heap objects and allows efficient batch rendering
 *  of many SVGs through a single manager.
 *
 *  @code
 *  vectorgl::SvgCache cache;
 *  int logo  = cache.loadFromString(svgLogoData);
 *  int icon  = cache.load("assets/icon.svg");
 *
 *  // Render loop
 *  cache.render(canvas, logo, 10, 10, 1.0f);
 *  cache.render(canvas, icon, 200, 10, 2.0f);
 *
 *  // Cleanup (or let destructor handle it)
 *  cache.remove(logo);
 *  cache.clear();
 *  @endcode
 *
 *  @sa SvgImage for a simpler single-SVG wrapper.
 */
class SvgCache
{
public:
    /*! @brief Sentinel value returned when loading fails. */
    static constexpr int kInvalidHandle = -1;

    SvgCache();
    ~SvgCache();
    SvgCache(SvgCache&&) noexcept;
    SvgCache& operator=(SvgCache&&) noexcept;
    SvgCache(const SvgCache&) = delete;
    SvgCache& operator=(const SvgCache&) = delete;

    /*! @brief Loads an SVG from a file on disk.
     *
     *  @param[in] path  Filesystem path to an `.svg` file (UTF-8).
     *  @return A non-negative handle on success, or @ref kInvalidHandle on failure.
     *
     *  @sa loadFromString, valid
     */
    [[nodiscard]] int load(const std::string& path);

    /*! @brief Loads an SVG from an in-memory string.
     *  Rejects malformed XML, inputs larger than 16 MiB, and excessive nesting or element counts.
     *
     *  @param[in] svgData  Complete SVG/XML document as a string.
     *  @return A non-negative handle on success, or @ref kInvalidHandle on failure.
     *
     *  @sa load, valid
     */
    [[nodiscard]] int loadFromString(const std::string& svgData);

    /*! @brief Renders a previously loaded SVG by handle.
     *
     *  The SVG is drawn with its top-left corner at (@p x, @p y), uniformly
     *  scaled by @p scale.  If @p handle is invalid the call is a safe no-op.
     *
     *  @param[in] canvas  The target canvas (must be between beginFrame/endFrame).
     *  @param[in] handle  Handle returned by @ref load or @ref loadFromString.
     *  @param[in] x       Horizontal position in canvas coordinates.
     *  @param[in] y       Vertical position in canvas coordinates.
     *  @param[in] scale   Uniform scale factor (default 1.0).
     */
    void render(Canvas& canvas, int handle, float x = 0, float y = 0, float scale = 1.0f) const;

    /*! @brief Returns the original SVG width for the given handle.
     *  @param[in] handle  A valid SVG handle.
     *  @return Width in SVG user units, or 0 if the handle is invalid.
     */
    [[nodiscard]] float width(int handle) const;

    /*! @brief Returns the original SVG height for the given handle.
     *  @param[in] handle  A valid SVG handle.
     *  @return Height in SVG user units, or 0 if the handle is invalid.
     */
    [[nodiscard]] float height(int handle) const;

    /*! @brief Checks whether a handle refers to a loaded SVG.
     *  @param[in] handle  The handle to check.
     *  @return `true` if @p handle was returned by a successful load call and
     *          has not been removed.
     */
    [[nodiscard]] bool valid(int handle) const;

    /*! @brief Removes a single SVG, freeing its memory.
     *
     *  The handle slot is recycled and may be reused by future load calls.
     *  Passing an invalid handle is a safe no-op.
     *
     *  @param[in] handle  The SVG to remove.
     */
    void remove(int handle);

    /*! @brief Removes all loaded SVGs and resets the cache.
     *
     *  All previously returned handles become invalid after this call.
     */
    void clear();

    /*! @brief Returns the number of currently loaded SVGs.
     *  @return Count of active (non-removed) entries.
     */
    [[nodiscard]] int count() const;

private:
    struct Impl;
    struct Slot;
    std::unique_ptr<Impl> impl_;
};

} // namespace vectorgl
