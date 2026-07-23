#include "vectorgl/image.hpp"

#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace vectorgl
{

Image::~Image()
{
    destroy();
}

Image::Image(Image&& other) noexcept : texture_(other.texture_), width_(other.width_), height_(other.height_)
{
    other.texture_ = 0;
    other.width_ = 0;
    other.height_ = 0;
}

Image& Image::operator=(Image&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        texture_ = other.texture_;
        width_ = other.width_;
        height_ = other.height_;
        other.texture_ = 0;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

bool Image::load(const std::string& path)
{
    destroy();

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data)
        return false;

    uint32_t texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
    texture_ = texture;
    width_ = width;
    height_ = height;
    return true;
}

void Image::destroy()
{
    if (texture_)
    {
        glDeleteTextures(1, &texture_);
    }
    texture_ = 0;
    width_ = 0;
    height_ = 0;
}

} // namespace vectorgl
