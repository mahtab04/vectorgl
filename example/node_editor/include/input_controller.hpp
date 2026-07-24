#pragma once

#include <vectorgl/path.hpp>
#include <string>

namespace vectorgl::node_editor
{

class GraphModel;
struct Edge;
struct Port;
struct Node;
struct EditorState;

class InputController
{
public:
	InputController() = default;

	void beginFrame(EditorState& state);
	void handlePointerMove(EditorState& state, GraphModel& model, float x, float y);
	void handlePointerDown(EditorState& state, GraphModel& model, float x, float y, int button);
	void handlePointerUp(EditorState& state, GraphModel& model, float x, float y, int button);
	void handleScroll(EditorState& state, float deltaX, float deltaY);
	void deleteSelected(GraphModel& model, EditorState& state);

private:
	Vec2 screenToWorld(const EditorState& state, const Vec2& screenPoint) const;
	bool hitTestPaletteItem(const Vec2& screenPoint, std::string& templateId) const;
	Node* hitTestNode(GraphModel& model, const EditorState& state, const Vec2& screenPoint) const;
	Edge* hitTestEdge(GraphModel& model, const EditorState& state, const Vec2& screenPoint) const;
	const Port* hitTestOutputPort(GraphModel& model, const EditorState& state, const Vec2& screenPoint,
	                             std::string& ownerNodeId) const;
	const Port* hitTestInputPort(GraphModel& model, const EditorState& state, const Vec2& screenPoint,
	                            std::string& ownerNodeId) const;
	void updateSelection(GraphModel& model, const std::string& selectedNodeId) const;
	void updateEdgeSelection(GraphModel& model, const std::string& selectedEdgeId) const;
};

} // namespace vectorgl::node_editor
