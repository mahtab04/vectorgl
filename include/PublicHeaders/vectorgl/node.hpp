#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "vectorgl/color.hpp"
#include "vectorgl/path.hpp"
#include "vectorgl/text.hpp"

namespace vectorgl
{
class Font;
class Image;

/*! @brief A 3×3 affine transform matrix for 2D operations.
 *
 *  Column-major layout: `[m0 m3 m6; m1 m4 m7; m2 m5 m8]`.
 */
struct Mat3x3
{
    std::array<float, 9> m{1, 0, 0, 0, 1, 0, 0, 0, 1};

    /*! @brief Returns the identity matrix. */
    static Mat3x3 identity();

    /*! @brief Returns a translation matrix. */
    static Mat3x3 translation(float tx, float ty);

    /*! @brief Returns a rotation matrix (clockwise, radians). */
    static Mat3x3 rotation(float angle);

    /*! @brief Returns a scaling matrix. */
    static Mat3x3 scaling(float sx, float sy);

    /*! @brief Matrix multiplication. */
    Mat3x3 operator*(const Mat3x3& o) const;

    /*! @brief Transforms a 2D point by this matrix. */
    Vec2 transformPoint(Vec2 p) const;
};

// Dirty flag levels
enum class DirtyFlag : uint8_t
{
    Clean = 0,
    TransformDirty = 1,
    StyleDirty = 2,
    GeometryDirty = 4,
    All = 7
};

inline DirtyFlag operator|(DirtyFlag a, DirtyFlag b)
{
    return static_cast<DirtyFlag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline DirtyFlag operator&(DirtyFlag a, DirtyFlag b)
{
    return static_cast<DirtyFlag>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline bool hasDirty(DirtyFlag flags, DirtyFlag test)
{
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(test)) != 0;
}

/*! @brief Shape types that can be rendered via SDF or tessellation. */
enum class ShapeType : uint8_t
{
    None,
    Rect,
    RoundedRect,
    Circle,
    Ellipse,
    Line,
    Path, // Tessellated complex path
    Text,
    Image,
    Group
};

/*! @brief Visual style applied to a Node (fill, stroke, opacity, corner radii). */
struct NodeStyle
{
    Color fillColor = Color::Transparent;
    Color strokeColor = Color::Transparent;
    float strokeWidth = 0.0f;
    float opacity = 1.0f;
    std::array<float, 4> cornerRadii = {0, 0, 0, 0}; // TL, TR, BR, BL
};

// Effect descriptors
struct ShadowEffect
{
    float blur = 0.0f;
    Vec2 offset{0, 0};
    Color color{0, 0, 0, 0.3f};
};

struct BlurEffect
{
    float radius = 0.0f;
};

struct GlowEffect
{
    float radius = 0.0f;
    Color color = Color::White;
};

struct NodeEffects
{
    ShadowEffect shadow;
    BlurEffect blur;
    GlowEffect glow;
    bool hasShadow = false;
    bool hasBlur = false;
    bool hasGlow = false;
};

/*! @brief A scene-graph node that can be rendered as an SDF shape, path, text, image, or group.
 *
 *  Nodes have a transform (position, size, rotation, scale), visual style
 *  (fill, stroke, opacity, corner radii), optional post-processing effects
 *  (shadow, blur, glow), and a parent/child hierarchy.
 *
 *  Create nodes through the Scene factory methods rather than directly.
 *
 *  @sa Scene
 */
class Node
{
public:
    Node() = default;
    explicit Node(ShapeType type) : type_(type) {}
    ~Node();

    // Hierarchy links and node IDs require stable object identity.
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    /*! @brief Returns the unique node identifier. */
    uint32_t id() const
    {
        return id_;
    }
    /*! @brief Returns the shape type. */
    ShapeType type() const
    {
        return type_;
    }

    // --- Transform ---

    /*! @brief Sets the node position.
     *  @param[in] x  X position.  @param[in] y  Y position.
     */
    void setPosition(float x, float y);

    /*! @brief Sets the node dimensions.
     *  @param[in] w  Width.  @param[in] h  Height.
     */
    void setSize(float w, float h);

    /*! @brief Sets the rotation angle.
     *  @param[in] radians  Clockwise rotation in radians.
     */
    void setRotation(float radians);

    /*! @brief Sets non-uniform scale.
     *  @param[in] sx  Horizontal scale.  @param[in] sy  Vertical scale.
     */
    void setScale(float sx, float sy);
    Vec2 position() const
    {
        return position_;
    }
    Vec2 size() const
    {
        return size_;
    }
    float rotation() const
    {
        return rotation_;
    }
    Vec2 scale() const
    {
        return scale_;
    }

    // --- Style ---

    /*! @brief Sets the fill color.
     *  @param[in] c  Fill color.
     */
    void setFill(Color c);

    /*! @brief Sets stroke color and width.
     *  @param[in] c      Stroke color.
     *  @param[in] width  Stroke width in pixels.
     */
    void setStroke(Color c, float width);

    /*! @brief Sets node opacity.
     *  @param[in] o  Opacity [0, 1].
     */
    void setOpacity(float o);

    /*! @brief Sets individual corner radii.
     *  @param[in] tl  Top-left.  @param[in] tr  Top-right.
     *  @param[in] br  Bottom-right.  @param[in] bl  Bottom-left.
     */
    void setCornerRadii(float tl, float tr, float br, float bl);

    /*! @brief Sets a uniform corner radius on all four corners.
     *  @param[in] r  Radius.
     */
    void setCornerRadius(float r);
    const NodeStyle& style() const
    {
        return style_;
    }
    NodeStyle& style()
    {
        return style_;
    }

    // --- Effects ---

    /*! @brief Applies a drop shadow effect.
     *  @param[in] blur    Shadow blur radius.
     *  @param[in] offset  Shadow offset.
     *  @param[in] color   Shadow color.
     */
    void setShadow(float blur, Vec2 offset, Color color);

    /*! @brief Applies a Gaussian blur effect.
     *  @param[in] radius  Blur radius.
     */
    void setBlur(float radius);

    /*! @brief Applies an outer glow effect.
     *  @param[in] radius  Glow radius.
     *  @param[in] color   Glow color.
     */
    void setGlow(float radius, Color color);

    /*! @brief Removes all effects from this node. */
    void clearEffects();
    const NodeEffects& effects() const
    {
        return effects_;
    }
    bool hasEffects() const
    {
        return effects_.hasShadow || effects_.hasBlur || effects_.hasGlow;
    }

    // --- Hierarchy ---

    /*! @brief Adds a child node.
     *  @param[in] child  Shared pointer to the child node.
     */
    void addChild(std::shared_ptr<Node> child);

    /*! @brief Removes a child node.
     *  @param[in] child  Shared pointer to the child to remove.
     */
    void removeChild(const std::shared_ptr<Node>& child);
    const std::vector<std::shared_ptr<Node>>& children() const
    {
        return children_;
    }
    Node* parent() const
    {
        return parent_;
    }
    int zIndex() const
    {
        return zIndex_;
    }
    /*! @brief Sets the z-index for render ordering.
     *  @param[in] z  Z-index value (higher = drawn later).
     */
    void setZIndex(int z);

    /*! @brief Sets the path geometry (only used when type() == ShapeType::Path).
     *  @param[in] path  The path to assign.
     */
    void setPath(const Path2D& path);
    const Path2D& path() const
    {
        return path_;
    }

    /*! Text nodes use a top-left layout origin. Size is controlled by the loaded
     * font and layout options, not setSize(). Resources are shared across nodes.
     * Release the scene and resources while their OpenGL context is current.
     * Flush the renderer before reloading/destroying a shared font in place.
     */
    void setText(std::string text);
    void setFont(std::shared_ptr<Font> font);
    void setTextLayout(const TextLayoutOptions& options);
    const std::string& text() const
    {
        return text_;
    }
    const std::shared_ptr<Font>& font() const
    {
        return font_;
    }
    const TextLayoutOptions& textLayoutOptions() const
    {
        return textOptions_;
    }
    /*! CPU-only layout; empty when no font is loaded. Text strokes are ignored. */
    TextLayout textLayout() const;
    void setTextPixelSnap(bool enabled);
    void setTextRenderingMode(TextRenderingMode mode);
    TextRenderingMode textRenderingMode() const
    {
        return textRenderingMode_;
    }
    bool textPixelSnap() const
    {
        return textPixelSnap_;
    }

    /*! Image nodes use a centered rectangle, like rect nodes. Zero dimensions
     * use the image's natural size; positive dimensions stretch the image.
     * Fill/stroke colors do not tint images; node opacity still applies.
     */
    void setImage(std::shared_ptr<Image> image);
    const std::shared_ptr<Image>& image() const
    {
        return image_;
    }
    Vec2 imageSize() const;

    // Dirty tracking
    DirtyFlag dirtyFlags() const
    {
        return dirty_;
    }
    void markDirty(DirtyFlag flags);
    void clearDirty()
    {
        dirty_ = DirtyFlag::Clean;
    }
    bool isDirty() const
    {
        return dirty_ != DirtyFlag::Clean;
    }

    // World transform (cached, recomputed when dirty)
    const Mat3x3& worldTransform() const
    {
        return worldTransform_;
    }
    void updateWorldTransform(const Mat3x3& parentTransform);

    // Visibility
    bool visible() const
    {
        return visible_;
    }
    void setVisible(bool v);

private:
    static inline std::atomic<uint32_t> nextId_{1};

    uint32_t id_ = nextId_.fetch_add(1, std::memory_order_relaxed);
    ShapeType type_ = ShapeType::None;
    Vec2 position_{0, 0};
    Vec2 size_{0, 0};
    float rotation_ = 0.0f;
    Vec2 scale_{1, 1};
    NodeStyle style_;
    NodeEffects effects_;
    Path2D path_;
    std::string text_;
    std::shared_ptr<Font> font_;
    TextLayoutOptions textOptions_;
    bool textPixelSnap_ = true;
    TextRenderingMode textRenderingMode_ = TextRenderingMode::Auto;
    std::shared_ptr<Image> image_;
    int zIndex_ = 0;
    bool visible_ = true;

    DirtyFlag dirty_ = DirtyFlag::All;
    Mat3x3 localTransform_ = Mat3x3::identity();
    Mat3x3 worldTransform_ = Mat3x3::identity();
    Mat3x3 cachedParentTransform_ = Mat3x3::identity();
    bool worldTransformDirty_ = true;

    Node* parent_ = nullptr;
    std::vector<std::shared_ptr<Node>> children_;

    void recomputeLocalTransform();
};

} // namespace vectorgl
