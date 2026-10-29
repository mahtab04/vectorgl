// ============================================================================
// svg_style.cpp — SVG style resolution
// ============================================================================

#include <cmath>
#include <cstdlib>
#include <sstream>

#include "svg_common.hpp"
#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{
namespace detail
{
namespace svg
{

namespace
{

// User-unit numbers and px lengths. Reject unsupported units as a whole;
// silently accepting a numeric prefix would change the dash pattern.
bool parseDashLength(const char*& p, float& value)
{
    char* end = nullptr;
    value = std::strtof(p, &end);
    if (end == p || !std::isfinite(value))
        return false;
    p = end;
    if (p[0] == 'p' && p[1] == 'x')
        p += 2;
    return true;
}

bool parseDashArray(const std::string& text, std::vector<float>& result)
{
    const std::string value = trimWhitespace(text);
    if (value == "none")
        return true;
    const char* p = value.c_str();
    if (!*p)
        return false;
    while (*p)
    {
        float length = 0;
        if (!parseDashLength(p, length) || length < 0)
            return false;
        result.push_back(length);
        if (!*p)
            return true;
        if (!isWhitespace(*p) && *p != ',')
            return false;
        while (isWhitespace(*p))
            ++p;
        if (*p == ',')
        {
            ++p;
            while (isWhitespace(*p))
                ++p;
            if (!*p)
                return false;
        }
    }
    return true;
}

/// Extract the ID from a url(#id) reference. Returns empty string if not a url() ref.
std::string extractUrlRef(const std::string& value)
{
    if (value.size() > 5 && value.substr(0, 4) == "url(" && value.back() == ')')
    {
        std::string inner = value.substr(4, value.size() - 5);
        // Trim whitespace and quotes
        while (!inner.empty() && (inner.front() == ' ' || inner.front() == '\'' || inner.front() == '"'))
            inner.erase(inner.begin());
        while (!inner.empty() && (inner.back() == ' ' || inner.back() == '\'' || inner.back() == '"'))
            inner.pop_back();
        // Strip leading '#'
        if (!inner.empty() && inner[0] == '#')
            inner = inner.substr(1);
        return inner;
    }
    return {};
}

void applyStyleProperty(Style& style, const std::string& property, const std::string& value)
{
    if (property == "fill")
    {
        std::string ref = extractUrlRef(value);
        if (!ref.empty())
        {
            style.fillRef = ref;
            style.hasFill = true;
        }
        else if (value == "none")
        {
            style.hasFill = false;
            style.fill = Color::Transparent;
            style.fillRef.clear();
        }
        else
        {
            style.hasFill = true;
            style.fill = parseColor(value);
            style.fillRef.clear();
        }
    }
    else if (property == "stroke")
    {
        std::string ref = extractUrlRef(value);
        if (!ref.empty())
        {
            style.strokeRef = ref;
            style.hasStroke = true;
        }
        else if (value == "none")
        {
            style.hasStroke = false;
            style.stroke = Color::Transparent;
            style.strokeRef.clear();
        }
        else
        {
            style.hasStroke = true;
            style.stroke = parseColor(value);
            style.strokeRef.clear();
        }
    }
    else if (property == "fill-rule")
    {
        if (value == "evenodd")
            style.fillRule = FillRule::EvenOdd;
        else if (value == "nonzero")
            style.fillRule = FillRule::NonZero;
    }
    else if (property == "stroke-width")
    {
        style.strokeWidth = std::strtof(value.c_str(), nullptr);
    }
    else if (property == "stroke-linejoin")
    {
        if (value == "miter")
            style.strokeStyle.join = LineJoin::Miter;
        else if (value == "round")
            style.strokeStyle.join = LineJoin::Round;
        else if (value == "bevel")
            style.strokeStyle.join = LineJoin::Bevel;
    }
    else if (property == "stroke-linecap")
    {
        if (value == "butt")
            style.strokeStyle.cap = LineCap::Butt;
        else if (value == "round")
            style.strokeStyle.cap = LineCap::Round;
        else if (value == "square")
            style.strokeStyle.cap = LineCap::Square;
    }
    else if (property == "stroke-miterlimit")
    {
        const auto number = trimWhitespace(value);
        char* end = nullptr;
        const float limit = std::strtof(number.c_str(), &end);
        if (end != number.c_str() && *end == '\0' && std::isfinite(limit) && limit >= 1)
            style.strokeStyle.miterLimit = limit;
    }
    else if (property == "stroke-dasharray")
    {
        std::vector<float> pattern;
        if (parseDashArray(value, pattern))
            style.strokeStyle.dashArray = std::move(pattern);
    }
    else if (property == "stroke-dashoffset")
    {
        const auto number = trimWhitespace(value);
        const char* p = number.c_str();
        float offset = 0;
        if (parseDashLength(p, offset) && !*p)
            style.strokeStyle.dashOffset = offset;
    }
    else if (property == "opacity")
    {
        style.opacity = std::strtof(value.c_str(), nullptr);
    }
    else if (property == "fill-opacity")
    {
        style.fillOpacity = std::strtof(value.c_str(), nullptr);
    }
    else if (property == "stroke-opacity")
    {
        style.strokeOpacity = std::strtof(value.c_str(), nullptr);
    }
    else if (property == "marker-start")
    {
        style.markerStart = extractUrlRef(value);
    }
    else if (property == "marker-mid")
    {
        style.markerMid = extractUrlRef(value);
    }
    else if (property == "marker-end")
    {
        style.markerEnd = extractUrlRef(value);
    }
}

void parseInlineStyle(Style& style, const std::string& styleAttr)
{
    std::istringstream stream(styleAttr);
    std::string declaration;
    while (std::getline(stream, declaration, ';'))
    {
        auto colon = declaration.find(':');
        if (colon == std::string::npos)
            continue;
        std::string property = trimWhitespace(declaration.substr(0, colon));
        std::string value = trimWhitespace(declaration.substr(colon + 1));
        applyStyleProperty(style, property, value);
    }
}

} // anonymous namespace

Style resolveStyle(const XmlElement& element, const Style& parentStyle, const std::string& stylesheet)
{
    Style style = parentStyle;
    // Don't inherit paint references from parent — they are not inherited per SVG spec
    // (Actually fill/stroke ARE inherited, but the url() reference should be re-evaluated)

    // XML attributes (lower priority than inline style)
    const auto& fillStr = getAttribute(element, "fill");
    if (!fillStr.empty())
        applyStyleProperty(style, "fill", fillStr);

    const auto& strokeStr = getAttribute(element, "stroke");
    if (!strokeStr.empty())
        applyStyleProperty(style, "stroke", strokeStr);

    const auto& sw = getAttribute(element, "stroke-width");
    if (!sw.empty())
        applyStyleProperty(style, "stroke-width", sw);

    for (const char* property :
         {"stroke-linejoin", "stroke-linecap", "stroke-miterlimit", "stroke-dasharray", "stroke-dashoffset"})
    {
        const auto& value = getAttribute(element, property);
        if (!value.empty())
            applyStyleProperty(style, property, value);
    }

    const auto& op = getAttribute(element, "opacity");
    if (!op.empty())
        applyStyleProperty(style, "opacity", op);

    const auto& fo = getAttribute(element, "fill-opacity");
    if (!fo.empty())
        applyStyleProperty(style, "fill-opacity", fo);

    const auto& so = getAttribute(element, "stroke-opacity");
    if (!so.empty())
        applyStyleProperty(style, "stroke-opacity", so);

    const auto& fr = getAttribute(element, "fill-rule");
    if (!fr.empty())
        applyStyleProperty(style, "fill-rule", fr);

    // Marker attributes
    const auto& ms = getAttribute(element, "marker-start");
    if (!ms.empty())
        applyStyleProperty(style, "marker-start", ms);
    const auto& mm = getAttribute(element, "marker-mid");
    if (!mm.empty())
        applyStyleProperty(style, "marker-mid", mm);
    const auto& me = getAttribute(element, "marker-end");
    if (!me.empty())
        applyStyleProperty(style, "marker-end", me);

    // Stylesheets override presentation attributes; inline style wins last.
    parseInlineStyle(style, stylesheet);

    // Inline style attribute (highest priority, overrides XML attributes)
    const auto& styleAttr = getAttribute(element, "style");
    if (!styleAttr.empty())
    {
        parseInlineStyle(style, styleAttr);
    }

    return style;
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
