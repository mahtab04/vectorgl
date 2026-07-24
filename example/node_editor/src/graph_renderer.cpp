#include "graph_renderer.hpp"

#include "editor_state.hpp"
#include "graph_model.hpp"
#include "theme.hpp"

#include <vectorgl/canvas.hpp>

namespace vectorgl::node_editor
{

namespace
{

constexpr float kCanvasExtent = 4000.0f;
constexpr float kHeaderHeight = 38.0f;
constexpr float kEdgeHandleLength = 80.0f;
constexpr float kPalettePanelX = 16.0f;
constexpr float kPalettePanelY = 16.0f;
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
    if (node.id == "node_input_color")
        return Color::hex(0x22C55E);
    if (node.id == "node_blend")
        return Color::hex(0xF97316);
    if (node.id == "node_output")
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

    for (float x = 0.0f; x <= kCanvasExtent; x += spacing)
    {
        canvas.beginPath();
        canvas.moveTo(x, 0.0f);
        canvas.lineTo(x, kCanvasExtent);
        canvas.stroke();
    }

    for (float y = 0.0f; y <= kCanvasExtent; y += spacing)
    {
        canvas.beginPath();
        canvas.moveTo(0.0f, y);
        canvas.lineTo(kCanvasExtent, y);
        canvas.stroke();
    }
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
    canvas.fillText("Node Palette", kPalettePanelX + 16.0f, kPalettePanelY + 27.0f);

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
        canvas.fillText(item.label, itemX + 48.0f, itemY + 31.0f);

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
        const float textY = center.y + applyZoom(4.0f, state.camera);
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
    canvas.fillText(node.title, position.x + applyZoom(14.0f, state.camera), position.y + applyZoom(24.0f, state.camera));

    drawPorts(canvas, node, node.inputs, state, theme, true);
    drawPorts(canvas, node, node.outputs, state, theme, false);
}

void GraphRenderer::render(Canvas& canvas, const GraphModel& model, const EditorState& state, const Theme& theme) const
{
    canvas.setFillColor(theme.backgroundColor);
    canvas.fillRect(0.0f, 0.0f, kCanvasExtent, kCanvasExtent);

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
}

} // namespace vectorgl::node_editor
