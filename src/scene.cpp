#include "vectorgl/scene.hpp"

#include <algorithm>
#include <functional>

#include "vectorgl/renderer.hpp"

namespace vectorgl
{

Scene::Scene() = default;
Scene::Scene(Renderer& renderer) : renderer_(&renderer) {}
Scene::~Scene() = default;

std::shared_ptr<Node> Scene::rect(float x, float y, float w, float h)
{
    auto node = std::make_shared<Node>(ShapeType::Rect);
    node->setPosition(x + w * 0.5f, y + h * 0.5f);
    node->setSize(w, h);
    addRoot(node);
    return node;
}

std::shared_ptr<Node> Scene::roundedRect(float x, float y, float w, float h, float radius)
{
    auto node = std::make_shared<Node>(ShapeType::RoundedRect);
    node->setPosition(x + w * 0.5f, y + h * 0.5f);
    node->setSize(w, h);
    node->setCornerRadius(radius);
    addRoot(node);
    return node;
}

std::shared_ptr<Node> Scene::circle(float cx, float cy, float r)
{
    auto node = std::make_shared<Node>(ShapeType::Circle);
    node->setPosition(cx, cy);
    node->setSize(r * 2, r * 2);
    addRoot(node);
    return node;
}

std::shared_ptr<Node> Scene::ellipse(float cx, float cy, float rx, float ry)
{
    auto node = std::make_shared<Node>(ShapeType::Ellipse);
    node->setPosition(cx, cy);
    node->setSize(rx * 2, ry * 2);
    addRoot(node);
    return node;
}

std::shared_ptr<Node> Scene::line(float x1, float y1, float x2, float y2)
{
    auto node = std::make_shared<Node>(ShapeType::Line);
    float cx = (x1 + x2) * 0.5f, cy = (y1 + y2) * 0.5f;
    float dx = x2 - x1, dy = y2 - y1;
    node->setPosition(cx, cy);
    node->setSize(std::sqrt(dx * dx + dy * dy), 0);
    node->setRotation(std::atan2(dy, dx));
    addRoot(node);
    return node;
}

std::shared_ptr<Node> Scene::path(const Path2D& p)
{
    auto node = std::make_shared<Node>(ShapeType::Path);
    node->setPath(p);
    addRoot(node);
    return node;
}

std::shared_ptr<Node> Scene::group()
{
    auto node = std::make_shared<Node>(ShapeType::Group);
    addRoot(node);
    return node;
}

void Scene::addRoot(std::shared_ptr<Node> node)
{
    if (!node || std::find(roots_.begin(), roots_.end(), node) != roots_.end())
        return;
    roots_.push_back(std::move(node));
}

void Scene::removeRoot(const std::shared_ptr<Node>& node)
{
    auto it = std::find(roots_.begin(), roots_.end(), node);
    if (it != roots_.end())
    {
        roots_.erase(it);
    }
}

std::shared_ptr<Node> Scene::findNode(uint32_t id) const
{
    std::function<std::shared_ptr<Node>(const std::shared_ptr<Node>&)> findRecursive;
    findRecursive = [&](const std::shared_ptr<Node>& node) -> std::shared_ptr<Node>
    {
        if (node->id() == id)
            return node;
        for (const auto& child : node->children())
            if (auto match = findRecursive(child))
                return match;
        return nullptr;
    };
    for (const auto& root : roots_)
        if (auto match = findRecursive(root))
            return match;
    return nullptr;
}

void Scene::update(float dt)
{
    // Update animations
    animator_.update(dt);
    animator_.applyTo(*this);

    // Update world transforms for all roots
    Mat3x3 identity = Mat3x3::identity();
    for (auto& root : roots_)
    {
        if (root->parent() == nullptr)
            root->updateWorldTransform(identity);
    }
}

void Scene::render(Renderer& renderer, int fbWidth, int fbHeight)
{
    renderer.beginFrame(fbWidth, fbHeight);

    std::vector<Node*> renderList;
    collectRenderList(renderList);

    // Sort by z-index
    std::stable_sort(renderList.begin(), renderList.end(),
                     [](const Node* a, const Node* b) { return a->zIndex() < b->zIndex(); });

    // Render each node
    for (Node* node : renderList)
    {
        renderer.renderNode(node);
    }

    // Flush any remaining SDF batch
    renderer.flushSDF();
    renderer.endFrame();
}

void Scene::render()
{
    if (!renderer_)
        return;

    std::vector<Node*> renderList;
    collectRenderList(renderList);

    std::stable_sort(renderList.begin(), renderList.end(),
                     [](const Node* a, const Node* b) { return a->zIndex() < b->zIndex(); });

    for (Node* node : renderList)
    {
        renderer_->renderNode(node);
    }
}

void Scene::collectRenderList(std::vector<Node*>& outList)
{
    for (auto& root : roots_)
    {
        // Factory-created nodes start as roots. Once attached to another node,
        // their parent owns traversal and they must not be rendered twice.
        if (root->parent() != nullptr)
            continue;
        collectNodes(root.get(), outList);
    }
}

void Scene::collectNodes(Node* node, std::vector<Node*>& outList)
{
    if (!node->visible())
        return;

    if (node->type() != ShapeType::Group && node->type() != ShapeType::None)
    {
        outList.push_back(node);
    }

    for (auto& child : node->children())
    {
        collectNodes(child.get(), outList);
    }
}

} // namespace vectorgl
