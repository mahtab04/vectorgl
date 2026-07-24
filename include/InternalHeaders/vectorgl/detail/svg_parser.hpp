// ============================================================================
// svg_parser.hpp — Internal SVG parsing infrastructure
// ============================================================================
// This is an internal header. Do not include it from public API headers.
// Provides: XML DOM, SVG color/style/transform/path parsing, and document
// tree rendering into a vectorgl::Canvas.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vectorgl/color.hpp"

namespace vectorgl
{

class Canvas; // Forward declaration

namespace detail
{
namespace svg
{

// ============================================================================
// XML DOM
// ============================================================================

struct XmlAttribute
{
    std::string name;
    std::string value;
};

struct XmlElement
{
    std::string tag;
    std::vector<XmlAttribute> attrs;
    std::vector<XmlElement> children;
    bool selfClosing = false;
};

/// Parse a complete XML document string into a root element whose children
/// are the top-level elements found in the input.
XmlElement parseXml(const std::string& xml);

/// Look up an attribute by name. Returns an empty string if not found.
const std::string& getAttribute(const XmlElement& element, const std::string& name);

/// Look up a float-valued attribute with a fallback default.
float getAttributeFloat(const XmlElement& element, const std::string& name, float defaultValue = 0.0f);

// ============================================================================
// SVG Color
// ============================================================================

/// Parse an SVG color string. Supports:
///   - Hex:    #RRGGBB, #RGB
///   - CSS:    rgb(r,g,b)
///   - Named:  60+ standard CSS color names
///   - "none": returns Color::Transparent
Color parseColor(const std::string& str);

// ============================================================================
// SVG Style
// ============================================================================

struct Style
{
    Color fill = Color::Black;
    Color stroke = Color::Transparent;
    float strokeWidth = 1.0f;
    float opacity = 1.0f;
    float fillOpacity = 1.0f;
    float strokeOpacity = 1.0f;
    bool hasFill = true;
    bool hasStroke = false;
    std::string fillRef;     // url(#id) reference for gradient/pattern fills
    std::string strokeRef;   // url(#id) reference for gradient/pattern strokes
    std::string markerStart; // url(#id) marker-start
    std::string markerMid;   // url(#id) marker-mid
    std::string markerEnd;   // url(#id) marker-end
};

/// Resolve the style for an element by inheriting from the parent style and
/// applying any fill/stroke/opacity attributes or inline `style=""` properties.
Style resolveStyle(const XmlElement& element, const Style& parentStyle);

// ============================================================================
// SVG Transform
// ============================================================================

/// A 2D affine transform stored as a 2×3 matrix:
///   | a  c  e |
///   | b  d  f |
struct Transform
{
    float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;

    static Transform identity()
    {
        return {};
    }

    /// Multiply two transforms: result = lhs * rhs
    static Transform multiply(const Transform& lhs, const Transform& rhs);
};

/// Parse the SVG `transform` attribute value. Supports translate, rotate,
/// scale, matrix, skewX, skewY — including chained transforms.
Transform parseTransform(const std::string& str);

// ============================================================================
// SVG ViewBox
// ============================================================================

struct ViewBox
{
    float x = 0, y = 0, w = 0, h = 0;
    bool valid = false;
};

ViewBox parseViewBox(const std::string& str);

// ============================================================================
// SVG Gradient
// ============================================================================

enum class GradientUnits : uint8_t
{
    ObjectBoundingBox,
    UserSpaceOnUse
};

struct GradientStop
{
    float offset = 0.0f;
    Color color = Color::Black;
    float opacity = 1.0f;
};

struct GradientDef
{
    std::string id;
    bool isRadial = false;
    GradientUnits units = GradientUnits::ObjectBoundingBox;
    // Linear: x1, y1, x2, y2
    float x1 = 0, y1 = 0, x2 = 1, y2 = 0;
    // Radial: cx, cy, r, fx, fy
    float cx = 0.5f, cy = 0.5f, r = 0.5f, fx = -1, fy = -1; // fx/fy < 0 means use cx/cy
    Transform gradientTransform;
    std::vector<GradientStop> stops;
    std::string href; // xlink:href to inherit from
};

/// Parse a <linearGradient> or <radialGradient> element into a GradientDef.
GradientDef parseGradientElement(const XmlElement& element);

// ============================================================================
// SVG Marker
// ============================================================================

struct MarkerDef
{
    std::string id;
    float refX = 0, refY = 0;
    float markerWidth = 3, markerHeight = 3;
    bool orientAuto = true;
    float orientAngle = 0;
    ViewBox viewBox;
    XmlElement content; // The marker's child elements to render
};

/// Parse a <marker> element into a MarkerDef.
MarkerDef parseMarkerElement(const XmlElement& element);

// ============================================================================
// SVG CSS Style Blocks
// ============================================================================

struct CssRule
{
    std::string selector;     // Simple selector: tag, .class, #id, or combinations
    std::string declarations; // "property: value; property: value; ..."
};

/// Parse a <style> element's text content into CSS rules.
std::vector<CssRule> parseCssStyleBlock(const std::string& cssText);

/// Check if a CSS selector matches an XML element.
bool cssSelectorMatches(const std::string& selector, const XmlElement& element);

// ============================================================================
// SVG Render Context
// ============================================================================

/// Holds resolved definitions (gradients, markers, CSS rules) for rendering.
struct SvgRenderContext
{
    std::vector<GradientDef> gradients;
    std::vector<MarkerDef> markers;
    std::vector<CssRule> cssRules;

    const GradientDef* findGradient(const std::string& id) const;
    const MarkerDef* findMarker(const std::string& id) const;
};

/// Collect all defs (gradients, markers, CSS rules) from the SVG tree.
SvgRenderContext collectDefs(const XmlElement& svgRoot);

// ============================================================================
// SVG Path
// ============================================================================

/// Parse the SVG path `d` attribute and emit corresponding Canvas path
/// commands. Supports all SVG path commands:
///   M/m, L/l, H/h, V/v, C/c, S/s, Q/q, T/t, A/a, Z/z
void emitPathCommands(const std::string& pathData, Canvas& canvas);

// ============================================================================
// SVG Document Rendering
// ============================================================================

/// Recursively render an SVG element tree into a Canvas, applying inherited
/// styles and composed transforms. Supports: path, rect, circle, ellipse,
/// line, polyline, polygon, text, image, use, g (groups).
/// Skips: defs, clipPath, mask, symbol, linearGradient, radialGradient, pattern.
/// Respects display="none" and visibility="hidden".
void renderElement(const XmlElement& element, Canvas& canvas, const Style& parentStyle,
                   const Transform& parentTransform);

/// Extended version that accepts the SVG root element for resolving <use> references.
void renderElementWithRoot(const XmlElement& element, Canvas& canvas, const Style& parentStyle,
                           const Transform& parentTransform, const XmlElement* svgRoot);

/// Context-aware rendering that supports gradients, markers, and CSS styles.
void renderElementWithContext(const XmlElement& element, Canvas& canvas, const Style& parentStyle,
                              const Transform& parentTransform, const XmlElement* svgRoot, const SvgRenderContext& ctx);

} // namespace svg
} // namespace detail
} // namespace vectorgl
