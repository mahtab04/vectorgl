#pragma once

#include <string>
#include <vector>

#include <vectorgl/path.hpp>

namespace vectorgl::node_editor
{

enum class PortKind
{
	Input,
	Output,
};

struct Port
{
	std::string id;
	std::string name;
	PortKind kind = PortKind::Input;
	Vec2 localPosition{};
};

struct Node
{
	std::string id;
	std::string title;
	Vec2 position{};
	Vec2 size{220.0f, 140.0f};
	std::vector<Port> inputs;
	std::vector<Port> outputs;
	bool selected = false;
	bool collapsed = false;
};

struct Edge
{
	std::string id;
	std::string fromNodeId;
	std::string fromPortId;
	std::string toNodeId;
	std::string toPortId;
	bool selected = false;
};

struct Graph
{
	std::vector<Node> nodes;
	std::vector<Edge> edges;
};

class GraphModel
{
public:
	GraphModel() = default;

	void clear();
	void loadDemoGraph();
	Node* addNodeFromTemplate(const std::string& templateId, Vec2 position);

	Graph& graph();
	const Graph& graph() const;

	Node* findNode(const std::string& nodeId);
	const Node* findNode(const std::string& nodeId) const;

private:
	Graph graph_{};
	int nextNodeSerial_ = 1;
};

} // namespace vectorgl::node_editor
