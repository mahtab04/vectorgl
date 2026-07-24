#pragma once

#include "editor_state.hpp"
#include "graph_model.hpp"
#include "theme.hpp"

namespace vectorgl
{
class Canvas;
}

namespace vectorgl::node_editor
{

class GraphRenderer
{
public:
	GraphRenderer() = default;

	void render(Canvas& canvas, const GraphModel& model, const EditorState& state, const Theme& theme) const;

private:
	Vec2 applyCamera(const Vec2& point, const Camera2D& camera) const;
	float applyZoom(float value, const Camera2D& camera) const;
	Color accentForNode(const Node& node, const Theme& theme) const;
	Color borderColorForNode(const Node& node, const EditorState& state, const Theme& theme) const;
	const Port* findPort(const std::vector<Port>& ports, const std::string& portId) const;
	Vec2 portWorldPosition(const Node& node, const Port& port) const;
	void drawGrid(Canvas& canvas, const EditorState& state, const Theme& theme) const;
	void drawEdge(Canvas& canvas, const GraphModel& model, const Edge& edge, const EditorState& state,
	              const Theme& theme) const;
	void drawPreviewEdge(Canvas& canvas, const GraphModel& model, const EditorState& state, const Theme& theme) const;
	void drawPalette(Canvas& canvas, const EditorState& state, const Theme& theme) const;
	void drawPorts(Canvas& canvas, const Node& node, const std::vector<Port>& ports, const EditorState& state,
	               const Theme& theme, bool isInput) const;
	void drawNode(Canvas& canvas, const Node& node, const EditorState& state, const Theme& theme) const;
};

} // namespace vectorgl::node_editor
