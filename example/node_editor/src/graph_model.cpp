#include "graph_model.hpp"

namespace vectorgl::node_editor
{

namespace
{

Node makeNodeFromTemplate(const std::string& templateId, const std::string& nodeId, Vec2 position)
{
    Node node;
    node.id = nodeId;
    node.position = position;

    if (templateId == "template_input_color")
    {
        node.title = "Input Color";
        node.size = {220.0f, 120.0f};
        node.outputs.push_back({"port_out", "Color", PortKind::Output, {node.size.x, 48.0f}});
    }
    else if (templateId == "template_blend")
    {
        node.title = "Blend";
        node.size = {240.0f, 160.0f};
        node.inputs.push_back({"port_color_a", "Color A", PortKind::Input, {0.0f, 48.0f}});
        node.inputs.push_back({"port_color_b", "Color B", PortKind::Input, {0.0f, 84.0f}});
        node.outputs.push_back({"port_result", "Result", PortKind::Output, {node.size.x, 48.0f}});
    }
    else if (templateId == "template_multiply")
    {
        node.title = "Multiply";
        node.size = {240.0f, 160.0f};
        node.inputs.push_back({"port_value_a", "Value A", PortKind::Input, {0.0f, 48.0f}});
        node.inputs.push_back({"port_value_b", "Value B", PortKind::Input, {0.0f, 84.0f}});
        node.outputs.push_back({"port_product", "Product", PortKind::Output, {node.size.x, 48.0f}});
    }
    else
    {
        node.title = "Output";
        node.size = {220.0f, 120.0f};
        node.inputs.push_back({"port_surface", "Surface", PortKind::Input, {0.0f, 48.0f}});
    }

    return node;
}

} // namespace

void GraphModel::clear()
{
    graph_.nodes.clear();
    graph_.edges.clear();
    nextNodeSerial_ = 1;
}

void GraphModel::loadDemoGraph()
{
    clear();

    Node inputColor = makeNodeFromTemplate("template_input_color", "node_input_color", {300.0f, 150.0f});
    inputColor.title = "Base Color";
    Node detailColor = makeNodeFromTemplate("template_input_color", "node_detail_color", {300.0f, 330.0f});
    detailColor.title = "Detail Color";
    Node blend = makeNodeFromTemplate("template_blend", "node_blend", {610.0f, 210.0f});
    Node output = makeNodeFromTemplate("template_output", "node_output", {950.0f, 250.0f});

    graph_.nodes.push_back(inputColor);
    graph_.nodes.push_back(detailColor);
    graph_.nodes.push_back(blend);
    graph_.nodes.push_back(output);

    graph_.edges.push_back({"edge_input_to_blend", "node_input_color", "port_out", "node_blend", "port_color_a"});
    graph_.edges.push_back({"edge_detail_to_blend", "node_detail_color", "port_out", "node_blend", "port_color_b"});
    graph_.edges.push_back({"edge_blend_to_output", "node_blend", "port_result", "node_output", "port_surface"});
    nextNodeSerial_ = 5;
}

Node* GraphModel::addNodeFromTemplate(const std::string& templateId, Vec2 position)
{
    const std::string nodeId = "node_generated_" + std::to_string(nextNodeSerial_++);
    graph_.nodes.push_back(makeNodeFromTemplate(templateId, nodeId, position));
    return &graph_.nodes.back();
}

Graph& GraphModel::graph()
{
    return graph_;
}

const Graph& GraphModel::graph() const
{
    return graph_;
}

Node* GraphModel::findNode(const std::string& nodeId)
{
    if (nodeId.empty())
        return nullptr;

    for (auto& node : graph_.nodes)
    {
        if (node.id == nodeId)
            return &node;
    }

    return nullptr;
}

const Node* GraphModel::findNode(const std::string& nodeId) const
{
    if (nodeId.empty())
        return nullptr;

    for (const auto& node : graph_.nodes)
    {
        if (node.id == nodeId)
            return &node;
    }

    return nullptr;
}

} // namespace vectorgl::node_editor
