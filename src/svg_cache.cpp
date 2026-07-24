// ============================================================================
// svg_cache.cpp — Handle-based SVG asset manager
// ============================================================================

#include "vectorgl/svg_cache.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

#include "vectorgl/canvas.hpp"
#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{

namespace svg = detail::svg;

struct SvgCache::Slot
{
    svg::XmlElement root;
    svg::ViewBox viewBox;
    float width = 0;
    float height = 0;
    bool occupied = false;
};

struct SvgCache::Impl
{
    std::vector<Slot> slots;
    std::vector<int> freeList; // recycled slot indices
    int activeCount = 0;

    int allocateSlot()
    {
        int idx;
        if (!freeList.empty())
        {
            idx = freeList.back();
            freeList.pop_back();
        }
        else
        {
            idx = static_cast<int>(slots.size());
            slots.emplace_back();
        }
        slots[idx].occupied = true;
        ++activeCount;
        return idx;
    }

    bool parseInto(Slot& slot, const std::string& svgData)
    {
        svg::XmlElement doc = svg::parseXml(svgData);

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

        slot.root = std::move(*svgElement);

        const auto& wStr = svg::getAttribute(slot.root, "width");
        const auto& hStr = svg::getAttribute(slot.root, "height");
        const auto& vbStr = svg::getAttribute(slot.root, "viewBox");

        slot.viewBox = svg::parseViewBox(vbStr);
        slot.width = !wStr.empty() ? std::strtof(wStr.c_str(), nullptr) : (slot.viewBox.valid ? slot.viewBox.w : 0);
        slot.height = !hStr.empty() ? std::strtof(hStr.c_str(), nullptr) : (slot.viewBox.valid ? slot.viewBox.h : 0);
        return true;
    }
};

SvgCache::SvgCache() : impl_(std::make_unique<Impl>()) {}
SvgCache::~SvgCache() = default;
SvgCache::SvgCache(SvgCache&&) noexcept = default;
SvgCache& SvgCache::operator=(SvgCache&&) noexcept = default;

int SvgCache::load(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return kInvalidHandle;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return loadFromString(content);
}

int SvgCache::loadFromString(const std::string& svgData)
{
    int idx = impl_->allocateSlot();
    if (!impl_->parseInto(impl_->slots[idx], svgData))
    {
        // Parse failed — free the slot immediately
        impl_->slots[idx] = Slot{};
        impl_->freeList.push_back(idx);
        --impl_->activeCount;
        return kInvalidHandle;
    }
    return idx;
}

void SvgCache::render(Canvas& canvas, int handle, float x, float y, float scale) const
{
    if (!valid(handle))
        return;

    const Slot& slot = impl_->slots[handle];

    canvas.save();
    canvas.translate(x, y);
    if (scale != 1.0f)
        canvas.scale(scale, scale);

    if (slot.viewBox.valid && slot.width > 0 && slot.height > 0)
    {
        float vbScale = std::min(slot.width / slot.viewBox.w, slot.height / slot.viewBox.h);
        canvas.translate(-slot.viewBox.x * vbScale, -slot.viewBox.y * vbScale);
        if (std::abs(vbScale - 1.0f) > 1e-4f)
        {
            canvas.scale(vbScale, vbScale);
        }
    }

    svg::Style defaultStyle;
    svg::Transform identity = svg::Transform::identity();

    for (const auto& child : slot.root.children)
    {
        svg::renderElementWithRoot(child, canvas, defaultStyle, identity, &slot.root);
    }

    canvas.restore();
}

float SvgCache::width(int handle) const
{
    return valid(handle) ? impl_->slots[handle].width : 0;
}

float SvgCache::height(int handle) const
{
    return valid(handle) ? impl_->slots[handle].height : 0;
}

bool SvgCache::valid(int handle) const
{
    return handle >= 0 && handle < static_cast<int>(impl_->slots.size()) && impl_->slots[handle].occupied;
}

void SvgCache::remove(int handle)
{
    if (!valid(handle))
        return;
    impl_->slots[handle] = Slot{};
    impl_->freeList.push_back(handle);
    --impl_->activeCount;
}

void SvgCache::clear()
{
    impl_->slots.clear();
    impl_->freeList.clear();
    impl_->activeCount = 0;
}

int SvgCache::count() const
{
    return impl_->activeCount;
}

} // namespace vectorgl
