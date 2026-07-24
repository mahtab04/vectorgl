// ============================================================================
// svg.cpp — SvgImage public API (delegates to svg_parser for all parsing)
// ============================================================================

#include "vectorgl/svg.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "vectorgl/canvas.hpp"
#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{

namespace svg = detail::svg;

struct SvgImage::Impl
{
    svg::XmlElement root;
    svg::ViewBox viewBox;
    float width = 0;
    float height = 0;
};

SvgImage::SvgImage() = default;
SvgImage::~SvgImage() = default;
SvgImage::SvgImage(SvgImage&&) noexcept = default;
SvgImage& SvgImage::operator=(SvgImage&&) noexcept = default;

bool SvgImage::load(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return loadFromString(content);
}

bool SvgImage::loadFromString(const std::string& svgData)
{
    svg::XmlElement doc = svg::parseXml(svgData);

    // Locate the <svg> root element
    svg::XmlElement* svgElement = nullptr;
    for (auto& child : doc.children)
    {
        if (child.tag == "svg")
        {
            svgElement = &child;
            break;
        }
    }
    if (!svgElement)
        return false;

    impl_ = std::make_unique<Impl>();
    impl_->root = std::move(*svgElement);

    // Resolve dimensions from width/height attributes and/or viewBox
    const auto& wStr = svg::getAttribute(impl_->root, "width");
    const auto& hStr = svg::getAttribute(impl_->root, "height");
    const auto& vbStr = svg::getAttribute(impl_->root, "viewBox");

    impl_->viewBox = svg::parseViewBox(vbStr);

    impl_->width = !wStr.empty() ? std::strtof(wStr.c_str(), nullptr) : (impl_->viewBox.valid ? impl_->viewBox.w : 0);
    impl_->height = !hStr.empty() ? std::strtof(hStr.c_str(), nullptr) : (impl_->viewBox.valid ? impl_->viewBox.h : 0);
    return true;
}

void SvgImage::render(Canvas& canvas, float x, float y, float scale) const
{
    if (!impl_)
        return;

    canvas.save();
    canvas.translate(x, y);
    if (scale != 1.0f)
        canvas.scale(scale, scale);

    // Apply viewBox → viewport mapping
    if (impl_->viewBox.valid && impl_->width > 0 && impl_->height > 0)
    {
        float vbScale = std::min(impl_->width / impl_->viewBox.w, impl_->height / impl_->viewBox.h);
        canvas.translate(-impl_->viewBox.x * vbScale, -impl_->viewBox.y * vbScale);
        if (std::abs(vbScale - 1.0f) > 1e-4f)
        {
            canvas.scale(vbScale, vbScale);
        }
    }

    svg::Style defaultStyle;
    svg::Transform identity = svg::Transform::identity();

    for (const auto& child : impl_->root.children)
    {
        svg::renderElementWithRoot(child, canvas, defaultStyle, identity, &impl_->root);
    }

    canvas.restore();
}

float SvgImage::width() const
{
    return impl_ ? impl_->width : 0;
}
float SvgImage::height() const
{
    return impl_ ? impl_->height : 0;
}
bool SvgImage::valid() const
{
    return impl_ != nullptr;
}
void SvgImage::destroy()
{
    impl_.reset();
}

} // namespace vectorgl
