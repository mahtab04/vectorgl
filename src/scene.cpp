#include "vectorgl/scene.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

#include "vectorgl/renderer.hpp"

namespace vectorgl
{
namespace
{
// Paths use the full affine transform; SDF shapes use the renderer's
// rotation/column-length decomposition (including its treatment of shear).
bool toLocal(Vec2 point, const Mat3x3& transform, Vec2& local)
{
    const double a = transform.m[0], b = transform.m[1];
    const double c = transform.m[3], d = transform.m[4];
    const double determinant = a * d - b * c;
    if (!std::isfinite(determinant) || determinant == 0)
        return false;
    const double x = point.x - transform.m[6], y = point.y - transform.m[7];
    local = {static_cast<float>((d * x - c * y) / determinant), static_cast<float>((a * y - b * x) / determinant)};
    return std::isfinite(local.x) && std::isfinite(local.y);
}

bool inSegment(Vec2 point, Vec2 a, Vec2 b, float halfWidth)
{
    const Vec2 delta = b - a;
    const double length = std::hypot(delta.x, delta.y);
    if (length == 0 || !std::isfinite(length))
        return false;
    const Vec2 offset = point - a;
    const double along = (offset.x * delta.x + offset.y * delta.y) / length;
    const double across = (offset.x * delta.y - offset.y * delta.x) / length;
    // Renderer strokes have butt caps and independent segment joins.
    return along >= 0 && along <= length && std::abs(across) <= halfWidth;
}

bool inPolygon(Vec2 point, const std::vector<Vec2>& polygon)
{
    if (polygon.size() < 3)
        return false;
    double area = 0;
    bool inside = false, boundary = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
    {
        const Vec2 a = polygon[j], b = polygon[i];
        area += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
        boundary = boundary || inSegment(point, a, b, 0);
        if ((a.y > point.y) != (b.y > point.y) &&
            point.x < (static_cast<double>(b.x) - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
            inside = !inside;
    }
    return area != 0 && (inside || boundary);
}

bool containsPoint(const Node& node, Vec2 point)
{
    const auto& style = node.style();
    if (!(style.opacity > 0))
        return false;
    const bool fill = style.fillColor.a > 0;
    const bool stroke = style.strokeColor.a > 0 && style.strokeWidth > 0;
    const auto& transform = node.worldTransform();
    const Vec2 size = node.size();

    if (node.type() == ShapeType::Path || node.type() == ShapeType::Line)
    {
        Vec2 local;
        if (!toLocal(point, transform, local))
            return false;
        if (node.type() == ShapeType::Line)
            return style.strokeColor.a > 0 && inSegment(local, {-size.x * 0.5f, 0}, {size.x * 0.5f, 0},
                                                        (style.strokeWidth > 0 ? style.strokeWidth : 1.0f) * 0.5f);
        if (!fill && !stroke)
            return false;
        // Each subpath is filled independently, as in Renderer::fillPath.
        for (const auto& polygon : node.path().getSubPaths())
        {
            if (fill && inPolygon(local, polygon))
                return true;
            if (stroke)
                for (size_t i = 1; i < polygon.size(); ++i)
                    if (inSegment(local, polygon[i - 1], polygon[i], style.strokeWidth * 0.5f))
                        return true;
        }
        return false;
    }
    if (!fill && !stroke)
        return false;
    const float sx = std::hypot(transform.m[0], transform.m[1]);
    const float sy = std::hypot(transform.m[3], transform.m[4]);
    if (!(sx > 0 && sy > 0) || !std::isfinite(sx + sy))
        return false;
    const float angle = std::atan2(transform.m[1], transform.m[0]);
    const float c = std::cos(angle), s = std::sin(angle);
    const float dx = point.x - transform.m[6], dy = point.y - transform.m[7];
    const Vec2 p{c * dx + s * dy, -s * dx + c * dy};
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        return false;
    const float hx = size.x * sx * 0.5f;
    const float hy = (node.type() == ShapeType::Circle ? size.x : size.y) * sy * 0.5f;
    if (!(hx > 0 && hy > 0) || !std::isfinite(hx + hy))
        return false;
    float distance;
    switch (node.type())
    {
    case ShapeType::Rect:
    case ShapeType::RoundedRect:
    {
        // Same quadrant selection and distance formula as shaders/sdf.frag.
        const size_t corner = p.x > 0 ? (p.y > 0 ? 0 : 1) : (p.y > 0 ? 2 : 3);
        const float radius = node.type() == ShapeType::RoundedRect ? style.cornerRadii[corner] * (sx + sy) * 0.5f : 0;
        const float qx = std::abs(p.x) - hx + radius, qy = std::abs(p.y) - hy + radius;
        distance = std::min(std::max(qx, qy), 0.0f) + std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) - radius;
        break;
    }
    case ShapeType::Circle:
    case ShapeType::Ellipse:
        if (node.type() == ShapeType::Circle && std::abs(sx - sy) < 1e-5f)
            distance = std::hypot(p.x, p.y) - hx;
        else
        {
            const float k0 = std::hypot(p.x / hx, p.y / hy);
            const float k1 = std::hypot(p.x / (hx * hx), p.y / (hy * hy));
            distance = k1 > 0 ? k0 * (k0 - 1) / k1 : -std::min(hx, hy);
        }
        break;
    default:
        return false;
    }
    return (fill && distance <= 0) || (stroke && std::abs(distance) <= style.strokeWidth * (sx + sy) * 0.25f);
}
} // namespace

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

std::shared_ptr<Node> Scene::pick(float x, float y)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return nullptr;
    for (const auto& root : roots_)
        if (!root->parent())
            root->updateWorldTransform(Mat3x3::identity());

    std::vector<Node*> nodes;
    collectRenderList(nodes);
    std::stable_sort(nodes.begin(), nodes.end(),
                     [](const Node* a, const Node* b) { return a->zIndex() < b->zIndex(); });
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
        if (containsPoint(**it, {x, y}))
            return findNode((*it)->id());
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
