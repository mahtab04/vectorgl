// ============================================================================
// svg_gradient.cpp — SVG gradient definition parsing
// ============================================================================

#include <cstdlib>

#include "svg_common.hpp"
#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{
namespace detail
{
namespace svg
{

GradientDef parseGradientElement(const XmlElement& element)
{
    GradientDef def;
    def.id = getAttribute(element, "id");
    def.isRadial = (element.tag == "radialGradient");

    const auto& units = getAttribute(element, "gradientUnits");
    if (units == "userSpaceOnUse")
        def.units = GradientUnits::UserSpaceOnUse;
    else
        def.units = GradientUnits::ObjectBoundingBox;

    const auto& gxf = getAttribute(element, "gradientTransform");
    if (!gxf.empty())
        def.gradientTransform = parseTransform(gxf);

    // Inherit from href
    const auto& href = getAttribute(element, "href");
    const auto& xlinkHref = getAttribute(element, "xlink:href");
    if (!href.empty())
        def.href = href;
    else if (!xlinkHref.empty())
        def.href = xlinkHref;
    // Strip leading '#'
    if (!def.href.empty() && def.href[0] == '#')
        def.href = def.href.substr(1);

    if (def.isRadial)
    {
        const auto& cxStr = getAttribute(element, "cx");
        const auto& cyStr = getAttribute(element, "cy");
        const auto& rStr = getAttribute(element, "r");
        const auto& fxStr = getAttribute(element, "fx");
        const auto& fyStr = getAttribute(element, "fy");

        def.cx = cxStr.empty() ? 0.5f : std::strtof(cxStr.c_str(), nullptr);
        def.cy = cyStr.empty() ? 0.5f : std::strtof(cyStr.c_str(), nullptr);
        def.r = rStr.empty() ? 0.5f : std::strtof(rStr.c_str(), nullptr);
        def.fx = fxStr.empty() ? -1.0f : std::strtof(fxStr.c_str(), nullptr);
        def.fy = fyStr.empty() ? -1.0f : std::strtof(fyStr.c_str(), nullptr);
    }
    else
    {
        const auto& x1Str = getAttribute(element, "x1");
        const auto& y1Str = getAttribute(element, "y1");
        const auto& x2Str = getAttribute(element, "x2");
        const auto& y2Str = getAttribute(element, "y2");

        def.x1 = x1Str.empty() ? 0.0f : std::strtof(x1Str.c_str(), nullptr);
        def.y1 = y1Str.empty() ? 0.0f : std::strtof(y1Str.c_str(), nullptr);
        def.x2 = x2Str.empty() ? 1.0f : std::strtof(x2Str.c_str(), nullptr);
        def.y2 = y2Str.empty() ? 0.0f : std::strtof(y2Str.c_str(), nullptr);
    }

    // Parse <stop> children
    for (const auto& child : element.children)
    {
        if (child.tag != "stop")
            continue;

        GradientStop stop;
        const auto& offsetStr = getAttribute(child, "offset");
        if (!offsetStr.empty())
        {
            if (offsetStr.back() == '%')
                stop.offset = std::strtof(offsetStr.c_str(), nullptr) / 100.0f;
            else
                stop.offset = std::strtof(offsetStr.c_str(), nullptr);
        }

        const auto& colorStr = getAttribute(child, "stop-color");
        if (!colorStr.empty())
            stop.color = parseColor(colorStr);

        const auto& opacStr = getAttribute(child, "stop-opacity");
        if (!opacStr.empty())
            stop.opacity = std::strtof(opacStr.c_str(), nullptr);

        // Check inline style for stop-color / stop-opacity
        const auto& styleStr = getAttribute(child, "style");
        if (!styleStr.empty())
        {
            // Simple parsing of "stop-color:...; stop-opacity:..."
            size_t pos = 0;
            while (pos < styleStr.size())
            {
                size_t semi = styleStr.find(';', pos);
                if (semi == std::string::npos)
                    semi = styleStr.size();
                std::string decl = styleStr.substr(pos, semi - pos);
                auto colon = decl.find(':');
                if (colon != std::string::npos)
                {
                    std::string prop = trimWhitespace(decl.substr(0, colon));
                    std::string val = trimWhitespace(decl.substr(colon + 1));
                    if (prop == "stop-color")
                        stop.color = parseColor(val);
                    else if (prop == "stop-opacity")
                        stop.opacity = std::strtof(val.c_str(), nullptr);
                }
                pos = semi + 1;
            }
        }

        stop.color.a *= stop.opacity;
        def.stops.push_back(stop);
    }

    return def;
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
