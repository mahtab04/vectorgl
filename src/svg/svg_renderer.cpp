// ============================================================================
// svg_renderer.cpp — SVG element rendering dispatch
// ============================================================================

#include <cmath>
#include <cstdlib>
#include <sstream>

#include "svg_common.hpp"
#include "vectorgl/canvas.hpp"
#include "vectorgl/detail/svg_parser.hpp"
#include "vectorgl/paint.hpp"
#include "vectorgl/renderer.hpp"

namespace vectorgl
{
namespace detail
{
namespace svg
{

namespace
{

/// Decompose a 2D affine transform and apply it to the canvas via
/// translate / rotate / scale.
void applyTransformToCanvas(Canvas& canvas, const Transform& xf)
{
    canvas.translate(xf.e, xf.f);

    float sx = std::sqrt(xf.a * xf.a + xf.b * xf.b);
    float sy = std::sqrt(xf.c * xf.c + xf.d * xf.d);
    float rot = std::atan2(xf.b, xf.a);

    if (std::abs(rot) > 1e-4f)
        canvas.rotate(rot);
    if (std::abs(sx - 1.0f) > 1e-4f || std::abs(sy - 1.0f) > 1e-4f)
    {
        canvas.scale(sx, sy);
    }
}

void applyFill(Canvas& canvas, const Style& style)
{
    Color fc = style.fill;
    fc.a *= style.fillOpacity * style.opacity;
    canvas.setFillColor(fc);
}

void applyStroke(Canvas& canvas, const Style& style)
{
    Color sc = style.stroke;
    sc.a *= style.strokeOpacity * style.opacity;
    canvas.setStrokeColor(sc);
    canvas.setLineWidth(style.strokeWidth);
}

float parseFontSize(const std::string& str)
{
    if (str.empty())
        return 16.0f;
    float val = std::strtof(str.c_str(), nullptr);
    if (val <= 0)
        val = 16.0f;
    return val;
}

std::string collectTextNodes(const XmlElement& element)
{
    std::string text;
    for (const auto& child : element.children)
    {
        if (child.tag.empty())
        {
            text += getAttribute(child, "text");
        }
    }
    return text;
}

bool isDefinitionTag(const std::string& tag)
{
    if (tag == "defs" || tag == "clipPath" || tag == "mask")
        return true;
    if (tag == "linearGradient" || tag == "radialGradient")
        return true;
    if (tag == "pattern" || tag == "marker" || tag == "style")
        return true;
    return false;
}

Transform computeUseTransform(const XmlElement& useElement, const XmlElement& target, const Transform& baseTransform)
{
    Transform useTransform = baseTransform;

    float useX = getAttributeFloat(useElement, "x");
    float useY = getAttributeFloat(useElement, "y");
    if (useX != 0.0f || useY != 0.0f)
    {
        Transform offset;
        offset.e = useX;
        offset.f = useY;
        useTransform = Transform::multiply(useTransform, offset);
    }

    if (target.tag == "symbol" || target.tag == "svg")
    {
        ViewBox viewBox = parseViewBox(getAttribute(target, "viewBox"));
        if (viewBox.valid && viewBox.w > 0.0f && viewBox.h > 0.0f)
        {
            float targetWidth = getAttributeFloat(target, "width", viewBox.w);
            float targetHeight = getAttributeFloat(target, "height", viewBox.h);
            float useWidth = getAttributeFloat(useElement, "width", targetWidth);
            float useHeight = getAttributeFloat(useElement, "height", targetHeight);

            if (useWidth <= 0.0f)
                useWidth = viewBox.w;
            if (useHeight <= 0.0f)
                useHeight = viewBox.h;

            Transform viewport;
            viewport.a = useWidth / viewBox.w;
            viewport.d = useHeight / viewBox.h;
            viewport.e = -viewBox.x * viewport.a;
            viewport.f = -viewBox.y * viewport.d;
            useTransform = Transform::multiply(useTransform, viewport);
        }
    }

    return useTransform;
}

/// Compute the bounding box of a shape element for objectBoundingBox gradient coords
struct BBox
{
    float x = 0, y = 0, w = 0, h = 0;
};

BBox computeShapeBBox(const XmlElement& element)
{
    BBox bb;
    const std::string& tag = element.tag;
    if (tag == "rect")
    {
        bb.x = getAttributeFloat(element, "x");
        bb.y = getAttributeFloat(element, "y");
        bb.w = getAttributeFloat(element, "width");
        bb.h = getAttributeFloat(element, "height");
    }
    else if (tag == "circle")
    {
        float cx = getAttributeFloat(element, "cx");
        float cy = getAttributeFloat(element, "cy");
        float r = getAttributeFloat(element, "r");
        bb.x = cx - r;
        bb.y = cy - r;
        bb.w = 2 * r;
        bb.h = 2 * r;
    }
    else if (tag == "ellipse")
    {
        float cx = getAttributeFloat(element, "cx");
        float cy = getAttributeFloat(element, "cy");
        float rx = getAttributeFloat(element, "rx");
        float ry = getAttributeFloat(element, "ry");
        bb.x = cx - rx;
        bb.y = cy - ry;
        bb.w = 2 * rx;
        bb.h = 2 * ry;
    }
    else if (tag == "line")
    {
        float x1 = getAttributeFloat(element, "x1");
        float y1 = getAttributeFloat(element, "y1");
        float x2 = getAttributeFloat(element, "x2");
        float y2 = getAttributeFloat(element, "y2");
        bb.x = std::min(x1, x2);
        bb.y = std::min(y1, y2);
        bb.w = std::abs(x2 - x1);
        bb.h = std::abs(y2 - y1);
    }
    return bb;
}

/// Convert a GradientDef to a Paint, resolving coordinates based on the bounding box.
Paint gradientToPaint(const GradientDef& grad, const BBox& bbox)
{
    Paint paint;
    if (grad.isRadial)
    {
        paint.type = PaintType::RadialGradient;
        float cx, cy, r;
        if (grad.units == GradientUnits::ObjectBoundingBox)
        {
            cx = bbox.x + grad.cx * bbox.w;
            cy = bbox.y + grad.cy * bbox.h;
            r = grad.r * std::max(bbox.w, bbox.h);
        }
        else
        {
            cx = grad.cx;
            cy = grad.cy;
            r = grad.r;
        }
        paint.gradientStart = {cx, cy};
        paint.innerRadius = 0.0f;
        paint.outerRadius = r;
    }
    else
    {
        paint.type = PaintType::LinearGradient;
        float x1, y1, x2, y2;
        if (grad.units == GradientUnits::ObjectBoundingBox)
        {
            x1 = bbox.x + grad.x1 * bbox.w;
            y1 = bbox.y + grad.y1 * bbox.h;
            x2 = bbox.x + grad.x2 * bbox.w;
            y2 = bbox.y + grad.y2 * bbox.h;
        }
        else
        {
            x1 = grad.x1;
            y1 = grad.y1;
            x2 = grad.x2;
            y2 = grad.y2;
        }
        paint.gradientStart = {x1, y1};
        paint.gradientEnd = {x2, y2};
    }

    // Convert gradient stops
    for (const auto& stop : grad.stops)
    {
        vectorgl::GradientStop gs;
        gs.position = stop.offset;
        gs.color = stop.color;
        paint.stops.push_back(gs);
    }

    if (!paint.stops.empty())
        paint.color = paint.stops[0].color;

    return paint;
}

/// Render markers at path vertices
void renderMarkers(const std::vector<Vec2>& points, const Style& style, Canvas& canvas, const Transform& xf,
                   const SvgRenderContext& ctx, const XmlElement* svgRoot)
{
    if (points.size() < 2)
        return;

    auto renderMarkerAt = [&](const MarkerDef& marker, Vec2 pos, float angle)
    {
        canvas.save();

        Transform markerXf = xf;
        Transform offset;
        offset.e = pos.x;
        offset.f = pos.y;
        markerXf = Transform::multiply(markerXf, offset);

        if (marker.orientAuto)
        {
            Transform rot;
            rot.a = std::cos(angle);
            rot.b = std::sin(angle);
            rot.c = -std::sin(angle);
            rot.d = std::cos(angle);
            markerXf = Transform::multiply(markerXf, rot);
        }
        else if (std::abs(marker.orientAngle) > 1e-6f)
        {
            Transform rot;
            rot.a = std::cos(marker.orientAngle);
            rot.b = std::sin(marker.orientAngle);
            rot.c = -std::sin(marker.orientAngle);
            rot.d = std::cos(marker.orientAngle);
            markerXf = Transform::multiply(markerXf, rot);
        }

        // Apply viewBox scaling if present
        float sx = 1.0f, sy = 1.0f;
        if (marker.viewBox.valid && marker.viewBox.w > 0 && marker.viewBox.h > 0)
        {
            sx = marker.markerWidth / marker.viewBox.w;
            sy = marker.markerHeight / marker.viewBox.h;
        }
        Transform scaleXf;
        scaleXf.a = sx;
        scaleXf.d = sy;
        markerXf = Transform::multiply(markerXf, scaleXf);

        // Offset by -refX, -refY
        Transform refOffset;
        refOffset.e = -marker.refX;
        refOffset.f = -marker.refY;
        markerXf = Transform::multiply(markerXf, refOffset);

        Style defaultStyle;
        renderElementWithContext(marker.content, canvas, defaultStyle, markerXf, svgRoot, ctx);

        canvas.restore();
    };

    // Start marker
    if (!style.markerStart.empty())
    {
        const MarkerDef* m = ctx.findMarker(style.markerStart);
        if (m && points.size() >= 2)
        {
            Vec2 dir = points[1] - points[0];
            float angle = std::atan2(dir.y, dir.x);
            renderMarkerAt(*m, points[0], angle);
        }
    }

    // Mid markers
    if (!style.markerMid.empty())
    {
        const MarkerDef* m = ctx.findMarker(style.markerMid);
        if (m)
        {
            for (size_t i = 1; i + 1 < points.size(); ++i)
            {
                Vec2 dirIn = points[i] - points[i - 1];
                Vec2 dirOut = points[i + 1] - points[i];
                float angleIn = std::atan2(dirIn.y, dirIn.x);
                float angleOut = std::atan2(dirOut.y, dirOut.x);
                float angle = (angleIn + angleOut) * 0.5f;
                renderMarkerAt(*m, points[i], angle);
            }
        }
    }

    // End marker
    if (!style.markerEnd.empty())
    {
        const MarkerDef* m = ctx.findMarker(style.markerEnd);
        if (m && points.size() >= 2)
        {
            Vec2 dir = points[points.size() - 1] - points[points.size() - 2];
            float angle = std::atan2(dir.y, dir.x);
            renderMarkerAt(*m, points.back(), angle);
        }
    }
}

void renderShapeElement(const XmlElement& element, Canvas& canvas, const Style& style, const Transform& xf,
                        const SvgRenderContext& ctx, const XmlElement* svgRoot)
{
    const std::string& tag = element.tag;

    // Check if we have a gradient fill
    const GradientDef* fillGradient = nullptr;
    if (!style.fillRef.empty())
        fillGradient = ctx.findGradient(style.fillRef);

    auto scoped = [&](auto drawFunc)
    {
        canvas.save();
        applyTransformToCanvas(canvas, xf);
        drawFunc();
        canvas.restore();
    };

    if (tag == "path")
    {
        const auto& d = getAttribute(element, "d");
        if (d.empty())
            return;
        scoped(
            [&]()
            {
                canvas.beginPath();
                emitPathCommands(d, canvas);
                if (style.hasFill)
                {
                    if (fillGradient)
                    {
                        BBox bbox = computeShapeBBox(element);
                        // For path, estimate bbox from path points — use a larger bbox
                        // The path data bbox isn't easily available, so use objectBoundingBox
                        // coordinates which default to 0-1 range covering the shape
                        Paint paint = gradientToPaint(*fillGradient, bbox);
                        float opacity = style.fillOpacity * style.opacity;
                        canvas.fillWithPaint(paint, opacity);
                    }
                    else
                    {
                        applyFill(canvas, style);
                        canvas.fill();
                    }
                }
                if (style.hasStroke)
                {
                    applyStroke(canvas, style);
                    canvas.stroke();
                }
            });
    }
    else if (tag == "rect")
    {
        float x = getAttributeFloat(element, "x");
        float y = getAttributeFloat(element, "y");
        float w = getAttributeFloat(element, "width");
        float h = getAttributeFloat(element, "height");
        float rx = getAttributeFloat(element, "rx");
        float ry = getAttributeFloat(element, "ry");
        if (ry == 0)
            ry = rx;
        if (rx == 0)
            rx = ry;

        scoped(
            [&]()
            {
                if (style.hasFill)
                {
                    if (fillGradient)
                    {
                        BBox bbox{x, y, w, h};
                        Paint paint = gradientToPaint(*fillGradient, bbox);
                        float opacity = style.fillOpacity * style.opacity;
                        // Build a rect path and fill with gradient
                        canvas.beginPath();
                        if (rx > 0)
                        {
                            // Rounded rect as path
                            canvas.moveTo(x + rx, y);
                            canvas.lineTo(x + w - rx, y);
                            canvas.arc(x + w - rx, y + rx, rx, -kPi * 0.5f, 0.0f);
                            canvas.lineTo(x + w, y + h - rx);
                            canvas.arc(x + w - rx, y + h - rx, rx, 0.0f, kPi * 0.5f);
                            canvas.lineTo(x + rx, y + h);
                            canvas.arc(x + rx, y + h - rx, rx, kPi * 0.5f, kPi);
                            canvas.lineTo(x, y + rx);
                            canvas.arc(x + rx, y + rx, rx, kPi, kPi * 1.5f);
                            canvas.closePath();
                        }
                        else
                        {
                            canvas.moveTo(x, y);
                            canvas.lineTo(x + w, y);
                            canvas.lineTo(x + w, y + h);
                            canvas.lineTo(x, y + h);
                            canvas.closePath();
                        }
                        canvas.fillWithPaint(paint, opacity);
                    }
                    else
                    {
                        applyFill(canvas, style);
                        if (rx > 0)
                            canvas.fillRoundedRect(x, y, w, h, rx);
                        else
                            canvas.fillRect(x, y, w, h);
                    }
                }
                if (style.hasStroke)
                {
                    applyStroke(canvas, style);
                    if (rx > 0)
                        canvas.strokeRoundedRect(x, y, w, h, rx);
                    else
                        canvas.strokeRect(x, y, w, h);
                }
            });
    }
    else if (tag == "circle")
    {
        float cx = getAttributeFloat(element, "cx");
        float cy = getAttributeFloat(element, "cy");
        float r = getAttributeFloat(element, "r");
        scoped(
            [&]()
            {
                if (style.hasFill)
                {
                    if (fillGradient)
                    {
                        BBox bbox{cx - r, cy - r, 2 * r, 2 * r};
                        Paint paint = gradientToPaint(*fillGradient, bbox);
                        float opacity = style.fillOpacity * style.opacity;
                        canvas.beginPath();
                        canvas.arc(cx, cy, r, 0.0f, 2.0f * kPi);
                        canvas.closePath();
                        canvas.fillWithPaint(paint, opacity);
                    }
                    else
                    {
                        applyFill(canvas, style);
                        canvas.fillCircle(cx, cy, r);
                    }
                }
                if (style.hasStroke)
                {
                    applyStroke(canvas, style);
                    canvas.strokeCircle(cx, cy, r);
                }
            });
    }
    else if (tag == "ellipse")
    {
        float cx = getAttributeFloat(element, "cx");
        float cy = getAttributeFloat(element, "cy");
        float rx = getAttributeFloat(element, "rx");
        float ry = getAttributeFloat(element, "ry");
        scoped(
            [&]()
            {
                if (style.hasFill)
                {
                    if (fillGradient)
                    {
                        BBox bbox{cx - rx, cy - ry, 2 * rx, 2 * ry};
                        Paint paint = gradientToPaint(*fillGradient, bbox);
                        float opacity = style.fillOpacity * style.opacity;
                        canvas.beginPath();
                        canvas.arc(cx, cy, 1.0f, 0.0f, 2.0f * kPi); // approximate
                        canvas.closePath();
                        // For ellipse, scale the path
                        canvas.fillWithPaint(paint, opacity);
                    }
                    else
                    {
                        applyFill(canvas, style);
                        canvas.fillEllipse(cx, cy, rx, ry);
                    }
                }
                if (style.hasStroke)
                {
                    applyStroke(canvas, style);
                    canvas.strokeEllipse(cx, cy, rx, ry);
                }
            });
    }
    else if (tag == "line")
    {
        float x1 = getAttributeFloat(element, "x1");
        float y1 = getAttributeFloat(element, "y1");
        float x2 = getAttributeFloat(element, "x2");
        float y2 = getAttributeFloat(element, "y2");
        if (style.hasStroke)
        {
            scoped(
                [&]()
                {
                    applyStroke(canvas, style);
                    canvas.beginPath();
                    canvas.moveTo(x1, y1);
                    canvas.lineTo(x2, y2);
                    canvas.stroke();
                });
        }
        // Render markers on line
        if (!style.markerStart.empty() || !style.markerEnd.empty())
        {
            std::vector<Vec2> pts = {{x1, y1}, {x2, y2}};
            renderMarkers(pts, style, canvas, xf, ctx, svgRoot);
        }
    }
    else if (tag == "polyline" || tag == "polygon")
    {
        const auto& pts = getAttribute(element, "points");
        if (pts.empty())
            return;

        // Parse points for both rendering and markers
        std::vector<Vec2> parsedPoints;
        const char* p = pts.c_str();
        while (*p)
        {
            skipWhitespaceAndCommas(p);
            if (!*p)
                break;
            char* end = nullptr;
            float x = std::strtof(p, &end);
            if (end == p)
                break;
            p = end;
            skipWhitespaceAndCommas(p);
            float y = std::strtof(p, &end);
            if (end == p)
                break;
            p = end;
            parsedPoints.push_back({x, y});
        }

        scoped(
            [&]()
            {
                canvas.beginPath();
                for (size_t i = 0; i < parsedPoints.size(); ++i)
                {
                    if (i == 0)
                        canvas.moveTo(parsedPoints[i].x, parsedPoints[i].y);
                    else
                        canvas.lineTo(parsedPoints[i].x, parsedPoints[i].y);
                }
                if (tag == "polygon")
                    canvas.closePath();
                if (style.hasFill && tag == "polygon")
                {
                    if (fillGradient)
                    {
                        BBox bbox = computeShapeBBox(element);
                        Paint paint = gradientToPaint(*fillGradient, bbox);
                        float opacity = style.fillOpacity * style.opacity;
                        canvas.fillWithPaint(paint, opacity);
                    }
                    else
                    {
                        applyFill(canvas, style);
                        canvas.fill();
                    }
                }
                if (style.hasStroke)
                {
                    applyStroke(canvas, style);
                    canvas.stroke();
                }
            });

        // Render markers
        if (!style.markerStart.empty() || !style.markerMid.empty() || !style.markerEnd.empty())
        {
            renderMarkers(parsedPoints, style, canvas, xf, ctx, svgRoot);
        }
    }
    else if (tag == "text")
    {
        float x = getAttributeFloat(element, "x");
        float y = getAttributeFloat(element, "y");
        const auto& fontSizeStr = getAttribute(element, "font-size");
        float fontSize = parseFontSize(fontSizeStr);
        const auto& fontFamily = getAttribute(element, "font-family");

        scoped(
            [&]()
            {
                if (!fontFamily.empty())
                {
                    canvas.setFont(fontFamily, fontSize);
                }
                if (style.hasFill)
                {
                    applyFill(canvas, style);
                }

                const std::string directText = collectTextNodes(element);
                if (!directText.empty())
                {
                    canvas.fillText(directText, x, y);
                }

                for (const auto& child : element.children)
                {
                    if (child.tag == "tspan")
                    {
                        Style childStyle = resolveStyle(child, style);
                        float tx = getAttributeFloat(child, "x", x);
                        float ty = getAttributeFloat(child, "y", y);
                        if (!getAttribute(child, "x").empty())
                            x = tx;
                        if (!getAttribute(child, "y").empty())
                            y = ty;

                        const std::string tspanText = collectTextNodes(child);
                        if (!tspanText.empty())
                        {
                            if (childStyle.hasFill)
                            {
                                applyFill(canvas, childStyle);
                            }
                            canvas.fillText(tspanText, tx, ty);
                        }
                    }
                }
            });
    }
    else if (tag == "image")
    {
        float x = getAttributeFloat(element, "x");
        float y = getAttributeFloat(element, "y");
        float w = getAttributeFloat(element, "width");
        float h = getAttributeFloat(element, "height");
        const auto& href = getAttribute(element, "href");
        const auto& xlinkHref = getAttribute(element, "xlink:href");
        const std::string& imgPath = !href.empty() ? href : xlinkHref;

        if (!imgPath.empty() && w > 0 && h > 0)
        {
            scoped(
                [&]()
                {
                    // Only load file paths, not data: URIs
                    if (imgPath.find("data:") != 0)
                    {
                        Image img = canvas.loadImage(imgPath);
                        canvas.drawImage(img, x, y, w, h);
                    }
                });
        }
    }
}

/// Find an element by id attribute recursively within a subtree.
const XmlElement* findElementById(const XmlElement& root, const std::string& id)
{
    const auto& elemId = getAttribute(root, "id");
    if (elemId == id)
        return &root;
    for (const auto& child : root.children)
    {
        const XmlElement* found = findElementById(child, id);
        if (found)
            return found;
    }
    return nullptr;
}

} // anonymous namespace

void renderElement(const XmlElement& element, Canvas& canvas, const Style& parentStyle,
                   const Transform& parentTransform)
{
    renderElementWithRoot(element, canvas, parentStyle, parentTransform, nullptr);
}

void renderElementWithRoot(const XmlElement& element, Canvas& canvas, const Style& parentStyle,
                           const Transform& parentTransform, const XmlElement* svgRoot)
{
    // Collect defs and delegate to the context-aware version
    SvgRenderContext ctx;
    if (svgRoot)
        ctx = collectDefs(*svgRoot);
    else
        ctx = collectDefs(element);
    renderElementWithContext(element, canvas, parentStyle, parentTransform, svgRoot, ctx);
}

void renderElementWithContext(const XmlElement& element, Canvas& canvas, const Style& parentStyle,
                              const Transform& parentTransform, const XmlElement* svgRoot, const SvgRenderContext& ctx)
{
    const std::string& tag = element.tag;

    // Skip <defs> — definitions are not rendered directly
    if (isDefinitionTag(tag))
    {
        return;
    }

    // Skip elements with display="none"
    const auto& display = getAttribute(element, "display");
    if (display == "none")
        return;

    // Skip elements with visibility="hidden" (but still process children for groups)
    const auto& visibility = getAttribute(element, "visibility");
    bool hidden = (visibility == "hidden" || visibility == "collapse");

    Style style = resolveStyle(element, parentStyle);

    // Apply CSS rules
    for (const auto& rule : ctx.cssRules)
    {
        if (cssSelectorMatches(rule.selector, element))
        {
            // Parse and apply the CSS declarations
            std::istringstream stream(rule.declarations);
            std::string declaration;
            while (std::getline(stream, declaration, ';'))
            {
                auto colon = declaration.find(':');
                if (colon == std::string::npos)
                    continue;
                std::string property = trimWhitespace(declaration.substr(0, colon));
                std::string value = trimWhitespace(declaration.substr(colon + 1));
                // Use the same style property application
                if (property == "fill")
                {
                    std::string ref;
                    if (value.size() > 5 && value.substr(0, 4) == "url(" && value.back() == ')')
                    {
                        std::string inner = value.substr(4, value.size() - 5);
                        while (!inner.empty() &&
                               (inner.front() == ' ' || inner.front() == '\'' || inner.front() == '"'))
                            inner.erase(inner.begin());
                        while (!inner.empty() && (inner.back() == ' ' || inner.back() == '\'' || inner.back() == '"'))
                            inner.pop_back();
                        if (!inner.empty() && inner[0] == '#')
                            inner = inner.substr(1);
                        ref = inner;
                    }
                    if (!ref.empty())
                    {
                        style.fillRef = ref;
                        style.hasFill = true;
                    }
                    else if (value == "none")
                    {
                        style.hasFill = false;
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
                    if (value == "none")
                    {
                        style.hasStroke = false;
                        style.strokeRef.clear();
                    }
                    else
                    {
                        style.hasStroke = true;
                        style.stroke = parseColor(value);
                    }
                }
                else if (property == "stroke-width")
                {
                    style.strokeWidth = std::strtof(value.c_str(), nullptr);
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
            }
        }
    }

    // Compose this element's transform with the parent
    Transform xf = parentTransform;
    const auto& tfStr = getAttribute(element, "transform");
    if (!tfStr.empty())
    {
        Transform local = parseTransform(tfStr);
        xf = Transform::multiply(parentTransform, local);
    }

    // Handle <use> element — clone referenced element
    if (tag == "use")
    {
        const auto& href = getAttribute(element, "href");
        const auto& xlinkHref = getAttribute(element, "xlink:href");
        std::string ref = !href.empty() ? href : xlinkHref;

        // Strip leading '#' from fragment reference
        if (!ref.empty() && ref[0] == '#')
            ref = ref.substr(1);

        if (!ref.empty() && svgRoot)
        {
            const XmlElement* target = findElementById(*svgRoot, ref);
            if (target)
            {
                Transform useTransform = computeUseTransform(element, *target, xf);

                if (target->tag == "symbol" || target->tag == "svg")
                {
                    Style targetStyle = resolveStyle(*target, style);
                    for (const auto& child : target->children)
                    {
                        renderElementWithContext(child, canvas, targetStyle, useTransform, svgRoot, ctx);
                    }
                }
                else
                {
                    renderElementWithContext(*target, canvas, style, useTransform, svgRoot, ctx);
                }
            }
        }
        return;
    }

    // Render this element if it's a drawable shape (and not hidden)
    if (!hidden)
    {
        renderShapeElement(element, canvas, style, xf, ctx, svgRoot);
    }

    // Recurse into children for all element types (groups, svg, etc.)
    for (const auto& child : element.children)
    {
        renderElementWithContext(child, canvas, style, xf, svgRoot ? svgRoot : &element, ctx);
    }
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
