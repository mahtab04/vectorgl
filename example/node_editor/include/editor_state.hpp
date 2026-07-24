#pragma once

#include <string>

#include <vectorgl/path.hpp>

namespace vectorgl::node_editor
{

enum class InteractionMode
{
	None,
	Panning,
	DraggingNode,
	MarqueeSelecting,
	ConnectingPort,
};

struct Camera2D
{
	Vec2 offset{};
	float zoom = 1.0f;
};

struct InteractionState
{
	InteractionMode mode = InteractionMode::None;
	std::string activeNodeId;
	std::string activePortId;
	Vec2 dragStart{};
	Vec2 currentMouse{};
};

struct EditorState
{
	Camera2D camera{};
	InteractionState interaction{};
	std::string hoveredEdgeId;
	std::string hoveredPaletteItemId;
	std::string hoveredNodeId;
	std::string hoveredPortNodeId;
	std::string hoveredPortId;
	std::string selectedEdgeId;
	std::string selectedNodeId;
	bool showGrid = true;
	bool showMiniMap = true;
};

} // namespace vectorgl::node_editor
