// ============================================================================
// svg_defs.cpp — SVG <defs> collection and render context
// ============================================================================

#include "svg_common.hpp"
#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{
namespace detail
{
namespace svg
{

const GradientDef* SvgRenderContext::findGradient(const std::string& id) const
{
    for (const auto& g : gradients)
    {
        if (g.id == id)
            return &g;
    }
    return nullptr;
}

const MarkerDef* SvgRenderContext::findMarker(const std::string& id) const
{
    for (const auto& m : markers)
    {
        if (m.id == id)
            return &m;
    }
    return nullptr;
}

namespace
{

void collectDefsRecursive(const XmlElement& element, SvgRenderContext& ctx)
{
    if (element.tag == "linearGradient" || element.tag == "radialGradient")
    {
        ctx.gradients.push_back(parseGradientElement(element));
    }
    else if (element.tag == "marker")
    {
        ctx.markers.push_back(parseMarkerElement(element));
    }
    else if (element.tag == "style")
    {
        // Collect text content from the style element
        // In our XML parser, text content is stored as children with empty tags
        // or the element might have a single text child. Let's handle both.
        std::string cssText;
        for (const auto& child : element.children)
        {
            // Text nodes in our parser are stored with tag="" and text in an attribute
            // Actually, let's check how our XML parser stores CDATA/text...
            // Our parser stores text as the "text" attribute on a pseudo-element
            const auto& text = getAttribute(child, "text");
            if (!text.empty())
                cssText += text;
        }
        // Also check for a direct "text" attribute on the style element itself
        const auto& directText = getAttribute(element, "text");
        if (!directText.empty())
            cssText += directText;

        // Try getting text from the first child if it has content stored differently
        // In most SVG files, style content is between <style> tags.
        // Our XML parser should store it. Let's also check for __text attribute.
        const auto& textContent = getAttribute(element, "__text");
        if (!textContent.empty())
            cssText += textContent;

        if (!cssText.empty())
        {
            auto rules = parseCssStyleBlock(cssText);
            ctx.cssRules.insert(ctx.cssRules.end(), rules.begin(), rules.end());
        }
    }

    for (const auto& child : element.children)
    {
        collectDefsRecursive(child, ctx);
    }
}

/// Resolve gradient inheritance (xlink:href)
void resolveGradientInheritance(SvgRenderContext& ctx)
{
    for (auto& grad : ctx.gradients)
    {
        if (grad.href.empty())
            continue;
        const GradientDef* parent = ctx.findGradient(grad.href);
        if (!parent)
            continue;
        // Inherit stops if this gradient has none
        if (grad.stops.empty())
            grad.stops = parent->stops;
    }
}

} // anonymous namespace

SvgRenderContext collectDefs(const XmlElement& svgRoot)
{
    SvgRenderContext ctx;
    collectDefsRecursive(svgRoot, ctx);
    resolveGradientInheritance(ctx);
    return ctx;
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
