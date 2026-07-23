#include "vectorgl/node.hpp"

#include <algorithm>
#include <cmath>

namespace vectorgl
{

// --- Mat3x3 ---

Mat3x3 Mat3x3::identity()
{
    return {};
}

Mat3x3 Mat3x3::translation(float tx, float ty)
{
    Mat3x3 r;
    r.m = {1, 0, 0, 0, 1, 0, tx, ty, 1};
    return r;
}

Mat3x3 Mat3x3::rotation(float angle)
{
    float c = std::cos(angle), s = std::sin(angle);
    Mat3x3 r;
    r.m = {c, s, 0, -s, c, 0, 0, 0, 1};
    return r;
}

Mat3x3 Mat3x3::scaling(float sx, float sy)
{
    Mat3x3 r;
    r.m = {sx, 0, 0, 0, sy, 0, 0, 0, 1};
    return r;
}

Mat3x3 Mat3x3::operator*(const Mat3x3& o) const
{
    Mat3x3 r;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
        {
            r.m[col * 3 + row] = 0;
            for (int k = 0; k < 3; ++k)
                r.m[col * 3 + row] += m[k * 3 + row] * o.m[col * 3 + k];
        }
    return r;
}

Vec2 Mat3x3::transformPoint(Vec2 p) const
{
    return {m[0] * p.x + m[3] * p.y + m[6], m[1] * p.x + m[4] * p.y + m[7]};
}

// --- Node ---

void Node::setPosition(float x, float y)
{
    if (position_.x != x || position_.y != y)
    {
        position_ = {x, y};
        markDirty(DirtyFlag::TransformDirty);
    }
}

void Node::setSize(float w, float h)
{
    if (size_.x != w || size_.y != h)
    {
        size_ = {w, h};
        markDirty(DirtyFlag::GeometryDirty);
    }
}

void Node::setRotation(float radians)
{
    if (rotation_ != radians)
    {
        rotation_ = radians;
        markDirty(DirtyFlag::TransformDirty);
    }
}

void Node::setScale(float sx, float sy)
{
    if (scale_.x != sx || scale_.y != sy)
    {
        scale_ = {sx, sy};
        markDirty(DirtyFlag::TransformDirty);
    }
}

void Node::setFill(Color c)
{
    if (style_.fillColor != c)
    {
        style_.fillColor = c;
        markDirty(DirtyFlag::StyleDirty);
    }
}

void Node::setStroke(Color c, float width)
{
    if (style_.strokeColor != c || style_.strokeWidth != width)
    {
        style_.strokeColor = c;
        style_.strokeWidth = width;
        markDirty(DirtyFlag::StyleDirty);
    }
}

void Node::setOpacity(float o)
{
    if (style_.opacity != o)
    {
        style_.opacity = o;
        markDirty(DirtyFlag::StyleDirty);
    }
}

void Node::setCornerRadii(float tl, float tr, float br, float bl)
{
    style_.cornerRadii[0] = tl;
    style_.cornerRadii[1] = tr;
    style_.cornerRadii[2] = br;
    style_.cornerRadii[3] = bl;
    markDirty(DirtyFlag::GeometryDirty);
}

void Node::setCornerRadius(float r)
{
    setCornerRadii(r, r, r, r);
}

void Node::setShadow(float blur, Vec2 offset, Color color)
{
    effects_.hasShadow = true;
    effects_.shadow = {blur, offset, color};
    markDirty(DirtyFlag::StyleDirty);
}

void Node::setBlur(float radius)
{
    effects_.hasBlur = true;
    effects_.blur = {radius};
    markDirty(DirtyFlag::StyleDirty);
}

void Node::setGlow(float radius, Color color)
{
    effects_.hasGlow = true;
    effects_.glow = {radius, color};
    markDirty(DirtyFlag::StyleDirty);
}

void Node::clearEffects()
{
    effects_ = {};
    markDirty(DirtyFlag::StyleDirty);
}

void Node::addChild(std::shared_ptr<Node> child)
{
    if (!child || child.get() == this || child->parent_ == this)
        return;

    // Reject cycles. Reparenting across an existing parent must be explicit
    // through removeChild() so the old parent's ownership remains coherent.
    for (Node* ancestor = this; ancestor != nullptr; ancestor = ancestor->parent_)
        if (ancestor == child.get())
            return;
    if (child->parent_ != nullptr)
        return;

    child->parent_ = this;
    child->markDirty(DirtyFlag::TransformDirty);
    children_.push_back(std::move(child));
}

void Node::removeChild(const std::shared_ptr<Node>& child)
{
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end())
    {
        (*it)->parent_ = nullptr;
        children_.erase(it);
    }
}

void Node::setZIndex(int z)
{
    if (zIndex_ != z)
    {
        zIndex_ = z;
        markDirty(DirtyFlag::TransformDirty);
    }
}

void Node::setPath(const Path2D& p)
{
    path_ = p;
    markDirty(DirtyFlag::GeometryDirty);
}

void Node::markDirty(DirtyFlag flags)
{
    dirty_ = dirty_ | flags;
    // Propagate transform dirty to children
    if (hasDirty(flags, DirtyFlag::TransformDirty))
    {
        for (auto& child : children_)
        {
            child->markDirty(DirtyFlag::TransformDirty);
        }
    }
}

void Node::setVisible(bool v)
{
    visible_ = v;
}

void Node::recomputeLocalTransform()
{
    localTransform_ = Mat3x3::translation(position_.x, position_.y) * Mat3x3::rotation(rotation_) *
                      Mat3x3::scaling(scale_.x, scale_.y);
}

void Node::updateWorldTransform(const Mat3x3& parentTransform)
{
    recomputeLocalTransform();
    worldTransform_ = parentTransform * localTransform_;
    for (auto& child : children_)
    {
        child->updateWorldTransform(worldTransform_);
    }
}

} // namespace vectorgl
