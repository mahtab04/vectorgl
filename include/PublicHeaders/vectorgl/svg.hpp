#pragma once
#include <memory>
#include <string>

namespace vectorgl
{

class Canvas;

/*! @brief A single SVG image loaded from file or string.
 *
 *  Each SvgImage owns its parsed SVG DOM tree. For managing many SVGs
 *  efficiently, prefer @ref SvgCache which uses lightweight integer handles.
 *
 *  @sa SvgCache
 */
class SvgImage
{
public:
    SvgImage();
    ~SvgImage();
    SvgImage(SvgImage&& other) noexcept;
    SvgImage& operator=(SvgImage&& other) noexcept;
    SvgImage(const SvgImage&) = delete;
    SvgImage& operator=(const SvgImage&) = delete;

    /*! @brief Loads an SVG from a file on disk.
     *
     *  @param[in] path  Filesystem path to an `.svg` file (UTF-8).
     *  @return `true` if the file was read and parsed successfully.
     *
     *  @note Any previously loaded content is replaced.
     */
    [[nodiscard]] bool load(const std::string& path);

    /*! @brief Loads an SVG from an in-memory string.
     *  Rejects malformed XML, inputs larger than 16 MiB, and excessive nesting or element counts.
     *
     *  @param[in] svgData  Complete SVG/XML document as a string.
     *  @return `true` if the string was parsed successfully.
     *
     *  @note Any previously loaded content is replaced.
     */
    [[nodiscard]] bool loadFromString(const std::string& svgData);

    /*! @brief Renders the SVG onto a canvas.
     *
     *  The SVG is drawn with its top-left corner at (@p x, @p y), uniformly
     *  scaled by @p scale.  The canvas transform stack is preserved.
     *
     *  @param[in] canvas  The target canvas (must be between beginFrame/endFrame).
     *  @param[in] x       Horizontal position in canvas coordinates.
     *  @param[in] y       Vertical position in canvas coordinates.
     *  @param[in] scale   Uniform scale factor (default 1.0).
     */
    void render(Canvas& canvas, float x = 0, float y = 0, float scale = 1.0f) const;

    /*! @brief Returns the original SVG width in user units.
     *  @return Width from the `width` attribute or `viewBox`, or 0 if not loaded.
     */
    [[nodiscard]] float width() const;

    /*! @brief Returns the original SVG height in user units.
     *  @return Height from the `height` attribute or `viewBox`, or 0 if not loaded.
     */
    [[nodiscard]] float height() const;

    /*! @brief Checks whether an SVG is currently loaded.
     *  @return `true` if a valid SVG document is loaded.
     */
    [[nodiscard]] bool valid() const;

    /*! @brief Releases all resources and resets the image to an unloaded state. */
    void destroy();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vectorgl
