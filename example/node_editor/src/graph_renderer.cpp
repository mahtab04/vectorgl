#include "graph_renderer.hpp"

#include "editor_state.hpp"
#include "graph_model.hpp"
#include "theme.hpp"

#include <vectorgl/canvas.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace vectorgl::node_editor
{

namespace
{

constexpr float kCanvasWidth = 1280.0f;
constexpr float kCanvasHeight = 680.0f;
constexpr float kToolbarHeight = 64.0f;
constexpr float kStatusBarHeight = 34.0f;
constexpr float kHeaderHeight = 38.0f;
constexpr float kEdgeHandleLength = 80.0f;
constexpr float kPalettePanelX = 16.0f;
constexpr float kPalettePanelY = 80.0f;
constexpr float kPalettePanelWidth = 220.0f;
constexpr float kPaletteHeaderHeight = 42.0f;
constexpr float kPaletteItemHeight = 56.0f;
constexpr float kPaletteItemGap = 10.0f;
constexpr float kPaletteItemInset = 12.0f;

struct PaletteItem
{
    const char* id;
    const char* label;
    Color accent;
};

constexpr PaletteItem kPaletteItems[] = {
    {"template_input_color", "Input Color", Color::hex(0x22C55E)},
    {"template_blend", "Blend", Color::hex(0xF97316)},
    {"template_multiply", "Multiply", Color::hex(0xFACC15)},
    {"template_output", "Output", Color::hex(0x8B5CF6)},
};

float centeredTextY(const Canvas& canvas, float top, float height)
{
    return top + std::max(0.0f, (height - canvas.lineHeight()) * 0.5f);
}

} // namespace

Vec2 GraphRenderer::applyCamera(const Vec2& point, const Camera2D& camera) const
{
    return {(point.x + camera.offset.x) * camera.zoom, (point.y + camera.offset.y) * camera.zoom};
}

float GraphRenderer::applyZoom(float value, const Camera2D& camera) const
{
    return value * camera.zoom;
}

Color GraphRenderer::accentForNode(const Node& node, const Theme& theme) const
{
    if (node.title == "Input Color" || node.title == "Base Color" || node.title == "Detail Color")
        return Color::hex(0x22C55E);
    if (node.title == "Blend")
        return Color::hex(0xF97316);
    if (node.title == "Multiply")
        return Color::hex(0xFACC15);
    if (node.title == "Output")
        return Color::hex(0x8B5CF6);

    return theme.accentColor;
}

Color GraphRenderer::borderColorForNode(const Node& node, const EditorState& state, const Theme& theme) const
{
    if (node.selected || state.selectedNodeId == node.id)
        return theme.selectionColor;
    if (state.hoveredNodeId == node.id)
        return Color::lerp(theme.panelBorderColor, theme.accentColor, 0.65f);

    return theme.panelBorderColor;
}

const Port* GraphRenderer::findPort(const std::vector<Port>& ports, const std::string& portId) const
{
    for (const auto& port : ports)
    {
        if (port.id == portId)
            return &port;
    }

    return nullptr;
}

Vec2 GraphRenderer::portWorldPosition(const Node& node, const Port& port) const
{
    return node.position + port.localPosition;
}

void GraphRenderer::drawGrid(Canvas& canvas, const EditorState& state, const Theme& theme) const
{
    if (!state.showGrid)
        return;

    const float spacing = applyZoom(theme.gridSpacing, state.camera);
    if (spacing <= 1.0f)
        return;

    canvas.setStrokeColor(theme.gridColor);
    canvas.setLineWidth(1.0f);

    float originX = std::fmod(state.camera.offset.x * state.camera.zoom, spacing);
    float originY = std::fmod(state.camera.offset.y * state.camera.zoom, spacing);
    if (originX < 0.0f)
        originX += spacing;
    if (originY < 0.0f)
        originY += spacing;

    for (float x = originX; x <= kCanvasWidth; x += spacing)
    {
        canvas.beginPath();
        canvas.moveTo(x, kToolbarHeight);
        canvas.lineTo(x, kCanvasHeight - kStatusBarHeight);
        canvas.stroke();
    }

    for (float y = originY; y <= kCanvasHeight - kStatusBarHeight; y += spacing)
    {
        if (y < kToolbarHeight)
            continue;
        canvas.beginPath();
        canvas.moveTo(0.0f, y);
        canvas.lineTo(kCanvasWidth, y);
        canvas.stroke();
    }
}

void GraphRenderer::drawToolbar(Canvas& canvas, const GraphModel& model, const EditorState& state,
                                const Theme& theme) const
{
    canvas.setFillColor(Color::hex(0x0D1528, 0.98f));
    canvas.fillRect(0.0f, 0.0f, kCanvasWidth, kToolbarHeight);
    canvas.setStrokeColor(theme.panelBorderColor.withAlpha(0.8f));
    canvas.setLineWidth(1.0f);
    canvas.beginPath();
    canvas.moveTo(0.0f, kToolbarHeight);
    canvas.lineTo(kCanvasWidth, kToolbarHeight);
    canvas.stroke();

    canvas.setFillColor(theme.accentColor);
    canvas.fillRoundedRect(18.0f, 14.0f, 36.0f, 36.0f, 10.0f);
    canvas.setFillColor(Color::hex(0x07111F));
    canvas.fillText("N", 29.0f, centeredTextY(canvas, 14.0f, 36.0f));
    canvas.setFillColor(theme.titleColor);
    canvas.fillText("VECTORGL NODE LAB", 68.0f, 22.0f);
    canvas.setFillColor(theme.bodyTextColor);
    canvas.fillText("Material Graph", 68.0f, 43.0f);

    canvas.setFillColor(theme.panelColor);
    canvas.fillRoundedRect(916.0f, 14.0f, 344.0f, 36.0f, 10.0f);
    canvas.setFillColor(theme.bodyTextColor);
    canvas.fillText("R  Reset     Del  Remove     MMB  Pan", 934.0f, centeredTextY(canvas, 14.0f, 36.0f));

    (void)model;
    (void)state;
}

void GraphRenderer::drawMiniMap(Canvas& canvas, const GraphModel& model, const EditorState& state,
                                const Theme& theme) const
{
    if (!state.showMiniMap)
        return;

    constexpr float mapX = 1032.0f;
    constexpr float mapY = 480.0f;
    constexpr float mapWidth = 228.0f;
    constexpr float mapHeight = 150.0f;
    constexpr float scale = 0.14f;

    canvas.setFillColor(Color::hex(0x0A1122, 0.94f));
    canvas.fillRoundedRect(mapX, mapY, mapWidth, mapHeight, 14.0f);
    canvas.setStrokeColor(theme.panelBorderColor);
    canvas.setLineWidth(1.5f);
    canvas.strokeRoundedRect(mapX, mapY, mapWidth, mapHeight, 14.0f);
    canvas.setFillColor(theme.bodyTextColor);
    canvas.fillText("MINIMAP", mapX + 14.0f, centeredTextY(canvas, mapY, 34.0f));

    for (const auto& node : model.graph().nodes)
    {
        const Color accent = accentForNode(node, theme);
        canvas.setFillColor(accent.withAlpha(node.selected ? 0.95f : 0.55f));
        canvas.fillRoundedRect(mapX + 12.0f + node.position.x * scale, mapY + 48.0f + node.position.y * scale,
                               std::max(12.0f, node.size.x * scale), std::max(8.0f, node.size.y * scale), 3.0f);
    }

    const float viewX = mapX + 12.0f - state.camera.offset.x * scale;
    const float viewY = mapY + 48.0f - state.camera.offset.y * scale;
    canvas.setStrokeColor(theme.selectionColor.withAlpha(0.85f));
    canvas.setLineWidth(1.5f);
    canvas.strokeRoundedRect(viewX, viewY, (kCanvasWidth / state.camera.zoom) * scale,
                             ((kCanvasHeight - kToolbarHeight) / state.camera.zoom) * scale, 4.0f);
}

void GraphRenderer::drawStatusBar(Canvas& canvas, const GraphModel& model, const EditorState& state,
                                  const Theme& theme) const
{
    const float top = kCanvasHeight - kStatusBarHeight;
    canvas.setFillColor(Color::hex(0x0D1528, 0.98f));
    canvas.fillRect(0.0f, top, kCanvasWidth, kStatusBarHeight);
    canvas.setFillColor(theme.bodyTextColor);
    canvas.fillText(std::to_string(model.graph().nodes.size()) + " nodes   " +
                      std::to_string(model.graph().edges.size()) + " connections",
                    18.0f, centeredTextY(canvas, top, kStatusBarHeight));
    canvas.fillText("Zoom " + std::to_string(static_cast<int>(state.camera.zoom * 100.0f)) + "%", 1160.0f,
                    centeredTextY(canvas, top, kStatusBarHeight));
}

void GraphRenderer::drawEdge(Canvas& canvas, const GraphModel& model, const Edge& edge, const EditorState& state,
                             const Theme& theme) const
{
    const Node* fromNode = model.findNode(edge.fromNodeId);
    const Node* toNode = model.findNode(edge.toNodeId);
    if (fromNode == nullptr || toNode == nullptr)
        return;

    const Port* fromPort = findPort(fromNode->outputs, edge.fromPortId);
    const Port* toPort = findPort(toNode->inputs, edge.toPortId);
    if (fromPort == nullptr || toPort == nullptr)
        return;

    const Vec2 start = applyCamera(portWorldPosition(*fromNode, *fromPort), state.camera);
    const Vec2 end = applyCamera(portWorldPosition(*toNode, *toPort), state.camera);
    const float handleLength = applyZoom(kEdgeHandleLength, state.camera);
    const bool connectsSelected = fromNode->selected || toNode->selected || state.selectedNodeId == fromNode->id ||
      state.selectedNodeId == toNode->id;
    const bool isHovered = state.hoveredEdgeId == edge.id;

    canvas.setStrokeColor(edge.selected || connectsSelected ? theme.selectionColor
                                                            : (isHovered ? theme.accentColor
                                                                         : theme.accentColor.withAlpha(0.85f)));
    canvas.setLineWidth(edge.selected || connectsSelected ? 3.5f : (isHovered ? 3.0f : 2.5f));
    canvas.beginPath();
    canvas.moveTo(start.x, start.y);
    canvas.bezierCurveTo(start.x + handleLength, start.y, end.x - handleLength, end.y, end.x, end.y);
    canvas.stroke();
}

void GraphRenderer::drawPreviewEdge(Canvas& canvas, const GraphModel& model, const EditorState& state,
                                    const Theme& theme) const
{
    if (state.interaction.mode != InteractionMode::ConnectingPort || state.interaction.activeNodeId.empty() ||
        state.interaction.activePortId.empty())
        return;

    const Node* fromNode = model.findNode(state.interaction.activeNodeId);
    if (fromNode == nullptr)
        return;

    const Port* fromPort = findPort(fromNode->outputs, state.interaction.activePortId);
    if (fromPort == nullptr)
        return;

    const Vec2 start = applyCamera(portWorldPosition(*fromNode, *fromPort), state.camera);
    const Vec2 end = state.interaction.currentMouse;
    const float handleLength = applyZoom(kEdgeHandleLength, state.camera);

    canvas.setStrokeColor(theme.accentColor.withAlpha(0.7f));
    canvas.setLineWidth(2.5f);
    canvas.beginPath();
    canvas.moveTo(start.x, start.y);
    canvas.bezierCurveTo(start.x + handleLength, start.y, end.x - handleLength, end.y, end.x, end.y);
    canvas.stroke();
}

void GraphRenderer::drawPalette(Canvas& canvas, const EditorState& state, const Theme& theme) const
{
    const float panelHeight = kPaletteHeaderHeight + (static_cast<float>(std::size(kPaletteItems)) * kPaletteItemHeight) +
      (static_cast<float>(std::size(kPaletteItems) - 1) * kPaletteItemGap) + (kPaletteItemInset * 2.0f);

    canvas.setFillColor(theme.panelColor.withAlpha(0.96f));
    canvas.fillRoundedRect(kPalettePanelX, kPalettePanelY, kPalettePanelWidth, panelHeight, 18.0f);

    canvas.setStrokeColor(theme.panelBorderColor.withAlpha(0.95f));
    canvas.setLineWidth(2.0f);
    canvas.strokeRoundedRect(kPalettePanelX, kPalettePanelY, kPalettePanelWidth, panelHeight, 18.0f);

    canvas.setFillColor(theme.titleColor);
    canvas.fillText("Node Palette", kPalettePanelX + 16.0f,
                    centeredTextY(canvas, kPalettePanelY, kPaletteHeaderHeight));

    float itemY = kPalettePanelY + kPaletteHeaderHeight + kPaletteItemInset;
    const float itemX = kPalettePanelX + kPaletteItemInset;
    const float itemWidth = kPalettePanelWidth - (kPaletteItemInset * 2.0f);
    for (const auto& item : kPaletteItems)
    {
        const bool isHovered = state.hoveredPaletteItemId == item.id;

        canvas.setFillColor(isHovered ? Color::lerp(theme.panelColor, item.accent.withAlpha(0.2f), 0.45f)
                                      : theme.panelColor.withAlpha(0.9f));
        canvas.fillRoundedRect(itemX, itemY, itemWidth, kPaletteItemHeight, 14.0f);

        canvas.setStrokeColor(isHovered ? item.accent : theme.panelBorderColor.withAlpha(0.9f));
        canvas.setLineWidth(isHovered ? 2.5f : 1.5f);
        canvas.strokeRoundedRect(itemX, itemY, itemWidth, kPaletteItemHeight, 14.0f);

        canvas.setFillColor(item.accent.withAlpha(0.22f));
        canvas.fillRoundedRect(itemX + 8.0f, itemY + 8.0f, 28.0f, 40.0f, 10.0f);

        canvas.setFillColor(theme.titleColor);
        canvas.fillText(item.label, itemX + 48.0f, centeredTextY(canvas, itemY, kPaletteItemHeight));

        itemY += kPaletteItemHeight + kPaletteItemGap;
    }
}

void GraphRenderer::drawPorts(Canvas& canvas, const Node& node, const std::vector<Port>& ports,
                              const EditorState& state, const Theme& theme, bool isInput) const
{
    for (const auto& port : ports)
    {
        const Vec2 center = applyCamera(portWorldPosition(node, port), state.camera);
        const float radius = applyZoom(theme.portRadius, state.camera);
        const Color nodeAccent = accentForNode(node, theme);
        const bool isHoveredNode = state.hoveredNodeId == node.id;
        const bool isHoveredPort = state.hoveredPortNodeId == node.id && state.hoveredPortId == port.id;
        const bool isActivePort = state.interaction.activeNodeId == node.id && state.interaction.activePortId == port.id;

        canvas.setFillColor(isInput ? theme.bodyTextColor.withAlpha(isHoveredNode ? 1.0f : 0.85f)
                                    : nodeAccent.withAlpha(isHoveredNode ? 1.0f : 0.95f));
        canvas.fillCircle(center.x, center.y, radius);

        if (isHoveredPort || isActivePort)
        {
            canvas.setFillColor((isInput ? theme.selectionColor : nodeAccent).withAlpha(0.14f));
            canvas.fillCircle(center.x, center.y, radius + applyZoom(5.0f, state.camera));
        }

        canvas.setStrokeColor(isActivePort ? theme.selectionColor
                                           : (isHoveredPort ? nodeAccent : theme.panelBorderColor.withAlpha(0.9f)));
        canvas.setLineWidth(isHoveredPort || isActivePort ? 2.5f : 1.5f);
        canvas.strokeCircle(center.x, center.y, radius);

        canvas.setFillColor(theme.bodyTextColor);
        const float textX = isInput ? center.x + applyZoom(14.0f, state.camera) : center.x - applyZoom(56.0f, state.camera);
        const float portRowHeight = applyZoom(28.0f, state.camera);
        const float textY = centeredTextY(canvas, center.y - portRowHeight * 0.5f, portRowHeight);
        canvas.fillText(port.name, textX, textY);
    }
}

void GraphRenderer::drawNode(Canvas& canvas, const Node& node, const EditorState& state, const Theme& theme) const
{
    const Vec2 position = applyCamera(node.position, state.camera);
    const float width = applyZoom(node.size.x, state.camera);
    const float height = applyZoom(node.size.y, state.camera);
    const float radius = applyZoom(theme.nodeCornerRadius, state.camera);
    const float headerHeight = applyZoom(kHeaderHeight, state.camera);
    const bool isHovered = state.hoveredNodeId == node.id;
    const Color headerAccent = accentForNode(node, theme);
    const Color borderColor = borderColorForNode(node, state, theme);

    canvas.setFillColor(isHovered ? Color::lerp(theme.panelColor, theme.accentColor.withAlpha(0.18f), 0.35f) : theme.panelColor);
    canvas.fillRoundedRect(position.x, position.y, width, height, radius);

    if (isHovered || node.selected)
    {
        canvas.setFillColor((node.selected ? theme.selectionColor : headerAccent).withAlpha(0.12f));
        canvas.fillRoundedRect(position.x - applyZoom(4.0f, state.camera), position.y - applyZoom(4.0f, state.camera),
                               width + applyZoom(8.0f, state.camera), height + applyZoom(8.0f, state.camera),
                               radius + applyZoom(4.0f, state.camera));
    }

    canvas.setStrokeColor(borderColor);
    canvas.setLineWidth(node.selected ? 3.0f : (isHovered ? 2.5f : 2.0f));
    canvas.strokeRoundedRect(position.x, position.y, width, height, radius);

    canvas.setFillColor(headerAccent.withAlpha(node.selected ? 0.28f : 0.2f));
    canvas.fillRoundedRect(position.x, position.y, width, headerHeight, radius);

    canvas.setFillColor(theme.titleColor);
    canvas.fillText(node.title, position.x + applyZoom(14.0f, state.camera),
                    centeredTextY(canvas, position.y, headerHeight));

    drawPorts(canvas, node, node.inputs, state, theme, true);
    drawPorts(canvas, node, node.outputs, state, theme, false);
}

void GraphRenderer::render(Canvas& canvas, const GraphModel& model, const EditorState& state, const Theme& theme) const
{
    canvas.setFillColor(theme.backgroundColor);
    canvas.fillRect(0.0f, 0.0f, kCanvasWidth, kCanvasHeight);

    drawGrid(canvas, state, theme);

    for (const auto& edge : model.graph().edges)
    {
        drawEdge(canvas, model, edge, state, theme);
    }

    drawPreviewEdge(canvas, model, state, theme);

    for (const auto& node : model.graph().nodes)
    {
        drawNode(canvas, node, state, theme);
    }

    drawPalette(canvas, state, theme);
    drawMiniMap(canvas, model, state, theme);
    drawToolbar(canvas, model, state, theme);
    drawStatusBar(canvas, model, state, theme);
}

} // namespace vectorgl::node_editor
