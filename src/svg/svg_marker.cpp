// ============================================================================
// svg_marker.cpp — SVG marker definition parsing
// ============================================================================

#include <cstdlib>
#include <cstring>

#include "svg_common.hpp"
#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{
namespace detail
{
namespace svg
{

MarkerDef parseMarkerElement(const XmlElement& element)
{
    MarkerDef def;
    def.id = getAttribute(element, "id");

    const auto& refXStr = getAttribute(element, "refX");
    const auto& refYStr = getAttribute(element, "refY");
    def.refX = refXStr.empty() ? 0.0f : std::strtof(refXStr.c_str(), nullptr);
    def.refY = refYStr.empty() ? 0.0f : std::strtof(refYStr.c_str(), nullptr);

    const auto& mwStr = getAttribute(element, "markerWidth");
    const auto& mhStr = getAttribute(element, "markerHeight");
    def.markerWidth = mwStr.empty() ? 3.0f : std::strtof(mwStr.c_str(), nullptr);
    def.markerHeight = mhStr.empty() ? 3.0f : std::strtof(mhStr.c_str(), nullptr);

    const auto& orient = getAttribute(element, "orient");
    if (orient.empty() || orient == "auto" || orient == "auto-start-reverse")
    {
        def.orientAuto = true;
    }
    else
    {
        def.orientAuto = false;
        def.orientAngle = std::strtof(orient.c_str(), nullptr) * kPi / 180.0f;
    }

    const auto& vbStr = getAttribute(element, "viewBox");
    if (!vbStr.empty())
        def.viewBox = parseViewBox(vbStr);

    // Store the marker's content (child elements)
    def.content.tag = "g";
    def.content.children = element.children;

    return def;
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
