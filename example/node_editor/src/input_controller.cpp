
#include "input_controller.hpp"

#include "editor_state.hpp"
#include "graph_model.hpp"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>

namespace vectorgl::node_editor
{

namespace
{

constexpr float kMinZoom = 0.35f;
constexpr float kMaxZoom = 2.5f;
constexpr float kPortHitRadius = 14.0f;
constexpr float kEdgeHitThreshold = 10.0f;
constexpr int kEdgeSamples = 24;
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
};

constexpr PaletteItem kPaletteItems[] = {
	{"template_input_color", "Input Color"},
	{"template_blend", "Blend"},
	{"template_multiply", "Multiply"},
	{"template_output", "Output"},
};

float distancePointToSegment(Vec2 point, Vec2 start, Vec2 end)
{
	const Vec2 segment = end - start;
	const float segmentLengthSquared = segment.x * segment.x + segment.y * segment.y;
	if (segmentLengthSquared <= 0.0001f)
		return (point - start).length();

	const Vec2 toPoint = point - start;
	const float projection = std::clamp((toPoint.x * segment.x + toPoint.y * segment.y) / segmentLengthSquared, 0.0f, 1.0f);
	const Vec2 closest = start + segment * projection;
	return (point - closest).length();
}

Vec2 cubicPoint(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t)
{
	const float omt = 1.0f - t;
	return p0 * (omt * omt * omt) + p1 * (3.0f * omt * omt * t) + p2 * (3.0f * omt * t * t) + p3 * (t * t * t);
}

} // namespace

Vec2 InputController::screenToWorld(const EditorState& state, const Vec2& screenPoint) const
{
	return {screenPoint.x / state.camera.zoom - state.camera.offset.x,
			screenPoint.y / state.camera.zoom - state.camera.offset.y};
}

bool InputController::hitTestPaletteItem(const Vec2& screenPoint, std::string& templateId) const
{
	const float itemX = kPalettePanelX + kPaletteItemInset;
	const float itemWidth = kPalettePanelWidth - (kPaletteItemInset * 2.0f);
	float itemY = kPalettePanelY + kPaletteHeaderHeight + kPaletteItemInset;

	for (const auto& item : kPaletteItems)
	{
		if (screenPoint.x >= itemX && screenPoint.x <= itemX + itemWidth && screenPoint.y >= itemY &&
			screenPoint.y <= itemY + kPaletteItemHeight)
		{
			templateId = item.id;
			return true;
		}
		itemY += kPaletteItemHeight + kPaletteItemGap;
	}

	templateId.clear();
	return false;
}

Node* InputController::hitTestNode(GraphModel& model, const EditorState& state, const Vec2& screenPoint) const
{
	const Vec2 worldPoint = screenToWorld(state, screenPoint);

	auto& nodes = model.graph().nodes;
	for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
	{
		Node& node = *it;
		const float left = node.position.x;
		const float top = node.position.y;
		const float right = left + node.size.x;
		const float bottom = top + node.size.y;
		if (worldPoint.x >= left && worldPoint.x <= right && worldPoint.y >= top && worldPoint.y <= bottom)
			return &node;
	}

	return nullptr;
}

Edge* InputController::hitTestEdge(GraphModel& model, const EditorState& state, const Vec2& screenPoint) const
{
	const Vec2 worldPoint = screenToWorld(state, screenPoint);

	auto& edges = model.graph().edges;
	for (auto it = edges.rbegin(); it != edges.rend(); ++it)
	{
		Edge& edge = *it;
		const Node* fromNode = model.findNode(edge.fromNodeId);
		const Node* toNode = model.findNode(edge.toNodeId);
		if (fromNode == nullptr || toNode == nullptr)
			continue;

		const Port* fromPort = nullptr;
		for (const auto& port : fromNode->outputs)
		{
			if (port.id == edge.fromPortId)
			{
				fromPort = &port;
				break;
			}
		}

		const Port* toPort = nullptr;
		for (const auto& port : toNode->inputs)
		{
			if (port.id == edge.toPortId)
			{
				toPort = &port;
				break;
			}
		}

		if (fromPort == nullptr || toPort == nullptr)
			continue;

		const Vec2 start = fromNode->position + fromPort->localPosition;
		const Vec2 end = toNode->position + toPort->localPosition;
		const Vec2 cp1 = start + Vec2{80.0f, 0.0f};
		const Vec2 cp2 = end - Vec2{80.0f, 0.0f};

		Vec2 previous = start;
		for (int sampleIndex = 1; sampleIndex <= kEdgeSamples; ++sampleIndex)
		{
			const float t = static_cast<float>(sampleIndex) / static_cast<float>(kEdgeSamples);
			const Vec2 current = cubicPoint(start, cp1, cp2, end, t);
			if (distancePointToSegment(worldPoint, previous, current) <= kEdgeHitThreshold / state.camera.zoom)
				return &edge;
			previous = current;
		}
	}

	return nullptr;
}

const Port* InputController::hitTestOutputPort(GraphModel& model, const EditorState& state, const Vec2& screenPoint,
	                                            std::string& ownerNodeId) const
{
	const Vec2 worldPoint = screenToWorld(state, screenPoint);

	auto& nodes = model.graph().nodes;
	for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
	{
		Node& node = *it;
		for (const auto& port : node.outputs)
		{
			const Vec2 delta = worldPoint - (node.position + port.localPosition);
			if (delta.length() <= kPortHitRadius)
			{
				ownerNodeId = node.id;
				return &port;
			}
		}
	}

	ownerNodeId.clear();
	return nullptr;
}

const Port* InputController::hitTestInputPort(GraphModel& model, const EditorState& state, const Vec2& screenPoint,
	                                           std::string& ownerNodeId) const
{
	const Vec2 worldPoint = screenToWorld(state, screenPoint);

	auto& nodes = model.graph().nodes;
	for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
	{
		Node& node = *it;
		for (const auto& port : node.inputs)
		{
			const Vec2 delta = worldPoint - (node.position + port.localPosition);
			if (delta.length() <= kPortHitRadius)
			{
				ownerNodeId = node.id;
				return &port;
			}
		}
	}

	ownerNodeId.clear();
	return nullptr;
}

void InputController::updateSelection(GraphModel& model, const std::string& selectedNodeId) const
{
	for (auto& node : model.graph().nodes)
	{
		node.selected = (node.id == selectedNodeId);
	}
}

void InputController::updateEdgeSelection(GraphModel& model, const std::string& selectedEdgeId) const
{
	for (auto& edge : model.graph().edges)
	{
		edge.selected = (edge.id == selectedEdgeId);
	}
}

void InputController::beginFrame(EditorState& state)
{
	state.hoveredEdgeId.clear();
	state.hoveredPaletteItemId.clear();
	state.hoveredNodeId.clear();
	state.hoveredPortNodeId.clear();
	state.hoveredPortId.clear();
}

void InputController::handlePointerMove(EditorState& state, GraphModel& model, float x, float y)
{
	const Vec2 previousMouse = state.interaction.currentMouse;
	const Vec2 currentMouse{x, y};
	state.interaction.currentMouse = currentMouse;

	if (state.interaction.mode == InteractionMode::DraggingNode)
	{
		Node* activeNode = model.findNode(state.interaction.activeNodeId);
		if (activeNode != nullptr)
		{
			const Vec2 previousWorld = screenToWorld(state, previousMouse);
			const Vec2 currentWorld = screenToWorld(state, currentMouse);
			activeNode->position = activeNode->position + (currentWorld - previousWorld);
		}
	}
	else if (state.interaction.mode == InteractionMode::Panning)
	{
		const Vec2 delta = currentMouse - previousMouse;
		state.camera.offset = state.camera.offset + Vec2{delta.x / state.camera.zoom, delta.y / state.camera.zoom};
	}

	std::string hoveredOutputNodeId;
	const Port* hoveredOutput = hitTestOutputPort(model, state, currentMouse, hoveredOutputNodeId);
	std::string hoveredInputNodeId;
	const Port* hoveredInput = hitTestInputPort(model, state, currentMouse, hoveredInputNodeId);
	std::string hoveredPaletteItemId;
	if (hitTestPaletteItem(currentMouse, hoveredPaletteItemId))
		state.hoveredPaletteItemId = hoveredPaletteItemId;

	if (state.interaction.mode == InteractionMode::ConnectingPort)
	{
		if (hoveredInput != nullptr)
		{
			state.hoveredPortNodeId = hoveredInputNodeId;
			state.hoveredPortId = hoveredInput->id;
		}
	}
	else if (hoveredOutput != nullptr)
	{
		state.hoveredPortNodeId = hoveredOutputNodeId;
		state.hoveredPortId = hoveredOutput->id;
	}
	else if (hoveredInput != nullptr)
	{
		state.hoveredPortNodeId = hoveredInputNodeId;
		state.hoveredPortId = hoveredInput->id;
	}

	Edge* hoveredEdge = hitTestEdge(model, state, currentMouse);
	state.hoveredEdgeId = hoveredEdge != nullptr ? hoveredEdge->id : std::string{};

	Node* hoveredNode = hitTestNode(model, state, currentMouse);
	state.hoveredNodeId = hoveredNode != nullptr ? hoveredNode->id : std::string{};
}

void InputController::handlePointerDown(EditorState& state, GraphModel& model, float x, float y, int button)
{
	state.interaction.currentMouse = {x, y};
	state.interaction.dragStart = {x, y};

	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		std::string paletteTemplateId;
		if (hitTestPaletteItem({x, y}, paletteTemplateId))
		{
			const float spawnX = kPalettePanelX + kPalettePanelWidth + 40.0f + static_cast<float>((model.graph().nodes.size() % 4) * 36);
			const float spawnY = 120.0f + static_cast<float>((model.graph().nodes.size() % 5) * 42);
			Node* newNode = model.addNodeFromTemplate(paletteTemplateId, screenToWorld(state, {spawnX, spawnY}));
			if (newNode != nullptr)
			{
				state.selectedEdgeId.clear();
				state.selectedNodeId = newNode->id;
				state.interaction.activeNodeId = newNode->id;
				state.interaction.activePortId.clear();
				state.interaction.mode = InteractionMode::None;
				updateSelection(model, newNode->id);
				updateEdgeSelection(model, std::string{});
			}
			return;
		}

		std::string outputNodeId;
		const Port* hitOutputPort = hitTestOutputPort(model, state, {x, y}, outputNodeId);
		if (hitOutputPort != nullptr)
		{
			state.selectedEdgeId.clear();
			state.selectedNodeId = outputNodeId;
			state.interaction.activeNodeId = outputNodeId;
			state.interaction.activePortId = hitOutputPort->id;
			state.interaction.mode = InteractionMode::ConnectingPort;
			updateSelection(model, outputNodeId);
			updateEdgeSelection(model, std::string{});
			return;
		}

		Edge* hitEdge = hitTestEdge(model, state, {x, y});
		if (hitEdge != nullptr)
		{
			state.selectedNodeId.clear();
			state.selectedEdgeId = hitEdge->id;
			state.interaction.activeNodeId.clear();
			state.interaction.activePortId.clear();
			state.interaction.mode = InteractionMode::None;
			updateSelection(model, std::string{});
			updateEdgeSelection(model, hitEdge->id);
			return;
		}

		Node* hitNode = hitTestNode(model, state, {x, y});
		if (hitNode != nullptr)
		{
			state.selectedEdgeId.clear();
			state.selectedNodeId = hitNode->id;
			state.interaction.activeNodeId = hitNode->id;
			state.interaction.mode = InteractionMode::DraggingNode;
			updateSelection(model, hitNode->id);
			updateEdgeSelection(model, std::string{});
		}
		else
		{
			state.selectedEdgeId.clear();
			state.selectedNodeId.clear();
			state.interaction.activeNodeId.clear();
			state.interaction.activePortId.clear();
			state.interaction.mode = InteractionMode::None;
			updateSelection(model, std::string{});
			updateEdgeSelection(model, std::string{});
		}
	}
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
	{
		state.interaction.mode = InteractionMode::Panning;
		state.interaction.activeNodeId.clear();
	}
}

void InputController::handlePointerUp(EditorState& state, GraphModel& model, float x, float y, int button)
{
	state.interaction.currentMouse = {x, y};

	if (button == GLFW_MOUSE_BUTTON_LEFT && state.interaction.mode == InteractionMode::DraggingNode)
	{
		state.interaction.mode = InteractionMode::None;
		state.interaction.activeNodeId.clear();
		state.interaction.activePortId.clear();
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && state.interaction.mode == InteractionMode::ConnectingPort)
	{
		std::string targetNodeId;
		const Port* targetPort = hitTestInputPort(model, state, {x, y}, targetNodeId);
		if (targetPort != nullptr && !state.interaction.activeNodeId.empty() && !state.interaction.activePortId.empty() &&
			targetNodeId != state.interaction.activeNodeId)
		{
			bool duplicate = false;
			for (const auto& edge : model.graph().edges)
			{
				if (edge.fromNodeId == state.interaction.activeNodeId && edge.fromPortId == state.interaction.activePortId &&
					edge.toNodeId == targetNodeId && edge.toPortId == targetPort->id)
				{
					duplicate = true;
					break;
				}
			}

			if (!duplicate)
			{
				// Input ports accept a single connection. Reconnecting replaces
				// the previous source, matching common node-editor behavior.
				auto& edges = model.graph().edges;
				edges.erase(std::remove_if(edges.begin(), edges.end(), [&](const Edge& edge)
				                           { return edge.toNodeId == targetNodeId && edge.toPortId == targetPort->id; }),
				            edges.end());
				const std::string edgeId = "edge_" + state.interaction.activeNodeId + "_" + targetNodeId + "_" +
				  std::to_string(edges.size());
				edges.push_back(
				  {edgeId, state.interaction.activeNodeId, state.interaction.activePortId, targetNodeId, targetPort->id});
			}
		}

		state.interaction.mode = InteractionMode::None;
		state.interaction.activeNodeId.clear();
		state.interaction.activePortId.clear();
	}
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE && state.interaction.mode == InteractionMode::Panning)
	{
		state.interaction.mode = InteractionMode::None;
	}
}

void InputController::handleScroll(EditorState& state, float deltaX, float deltaY)
{
	(void)deltaX;

	const float oldZoom = state.camera.zoom;
	const float newZoom = std::clamp(oldZoom * (1.0f + deltaY * 0.12f), kMinZoom, kMaxZoom);
	if (newZoom == oldZoom)
		return;

	const Vec2 cursor = state.interaction.currentMouse;
	const Vec2 worldBefore = screenToWorld(state, cursor);
	state.camera.zoom = newZoom;
	state.camera.offset = {cursor.x / state.camera.zoom - worldBefore.x, cursor.y / state.camera.zoom - worldBefore.y};
}

void InputController::deleteSelected(GraphModel& model, EditorState& state)
{
    auto& graph = model.graph();

    if (!state.selectedNodeId.empty())
    {
        const std::string nodeId = state.selectedNodeId;
        graph.edges.erase(
          std::remove_if(graph.edges.begin(), graph.edges.end(), [&](const Edge& edge)
                         { return edge.fromNodeId == nodeId || edge.toNodeId == nodeId; }),
          graph.edges.end());
        graph.nodes.erase(std::remove_if(graph.nodes.begin(), graph.nodes.end(),
                                        [&](const Node& node) { return node.id == nodeId; }),
                          graph.nodes.end());
        state.selectedNodeId.clear();
        state.hoveredNodeId.clear();
        state.interaction = {};
        return;
    }

    if (!state.selectedEdgeId.empty())
    {
        graph.edges.erase(
          std::remove_if(graph.edges.begin(), graph.edges.end(),
                         [&](const Edge& edge) { return edge.id == state.selectedEdgeId; }),
          graph.edges.end());
        state.selectedEdgeId.clear();
        updateEdgeSelection(model, std::string{});
    }
}

} // namespace vectorgl::node_editor
