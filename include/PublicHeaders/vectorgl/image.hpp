#pragma once
#include <cstdint>
#include <string>

namespace vectorgl
{

/*! @brief A GPU-resident raster image (texture).
 *
 *  Load images from disk (PNG, JPG, BMP, TGA via stb_image) and draw them
 *  on a Canvas.  Move-only; not copyable.
 *
 *  @code
 *  vectorgl::Image img;
 *  img.load("photo.png");
 *  canvas.drawImage(img, 10, 10);
 *  img.destroy();  // or let destructor handle it
 *  @endcode
 */
class Image
{
public:
    Image() = default;
    ~Image();
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    /*! @brief Loads an image from a file and uploads it to the GPU.
     *  @param[in] path  Filesystem path to an image file (PNG, JPG, BMP, TGA).
     *  @return `true` if the image was loaded and uploaded successfully.
     */
    [[nodiscard]] bool load(const std::string& path);

    /*! @brief Releases the GPU texture. Safe to call multiple times. */
    void destroy();

    /*! @brief Returns the OpenGL texture ID (0 if not loaded). */
    [[nodiscard]] uint32_t texture() const
    {
        return texture_;
    }
    /*! @brief Returns the image width in pixels. */
    [[nodiscard]] int width() const
    {
        return width_;
    }
    /*! @brief Returns the image height in pixels. */
    [[nodiscard]] int height() const
    {
        return height_;
    }
    /*! @brief Checks whether a valid texture is loaded. */
    [[nodiscard]] bool valid() const
    {
        return texture_ != 0;
    }

private:
    uint32_t texture_ = 0;
    int width_ = 0, height_ = 0;
};

} // namespace vectorgl
