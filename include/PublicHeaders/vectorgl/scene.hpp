#pragma once
#include <memory>
#include <vector>

#include "vectorgl/animator.hpp"
#include "vectorgl/color.hpp"
#include "vectorgl/node.hpp"
#include "vectorgl/path.hpp"

namespace vectorgl
{

class Renderer;

/*! @brief Retained-mode scene graph for composing, animating, and rendering 2D nodes.
 *
 *  Provides factory methods to create typed nodes (rect, circle, etc.) and
 *  manages a tree of root nodes.  Call update() to advance animations, then
 *  render() to draw everything.
 *
 *  @code
 *  vectorgl::Scene scene(renderer);
 *  auto r = scene.rect(10, 10, 200, 100);
 *  r->setFill(vectorgl::Color::Blue);
 *
 *  // Each frame:
 *  scene.update(dt);
 *  scene.render();
 *  @endcode
 */
class Scene
{
public:
    Scene();

    /*! @brief Constructs a scene bound to a renderer.
     *  @param[in] renderer  The renderer used by the parameterless render() overload.
     */
    explicit Scene(Renderer& renderer);
    ~Scene();

    // --- Shape factories ---

    /*! @brief Creates a rectangle node and adds it to the scene roots.
     *  @param[in] x  Left.  @param[in] y  Top.
     *  @param[in] w  Width. @param[in] h  Height.
     */
    std::shared_ptr<Node> rect(float x, float y, float w, float h);

    /*! @brief Creates a rounded rectangle node and adds it to the scene roots.
     *  @param[in] x       Left.   @param[in] y       Top.
     *  @param[in] w       Width.  @param[in] h       Height.
     *  @param[in] radius  Corner radius.
     */
    std::shared_ptr<Node> roundedRect(float x, float y, float w, float h, float radius);

    /*! @brief Creates a circle node.
     *  @param[in] cx  Center X.  @param[in] cy  Center Y.  @param[in] r  Radius.
     */
    std::shared_ptr<Node> circle(float cx, float cy, float r);

    /*! @brief Creates an ellipse node.
     *  @param[in] cx  Center X.  @param[in] cy  Center Y.
     *  @param[in] rx  Horizontal radius.  @param[in] ry  Vertical radius.
     */
    std::shared_ptr<Node> ellipse(float cx, float cy, float rx, float ry);

    /*! @brief Creates a line node.
     *  @param[in] x1,y1  Start point.
     *  @param[in] x2,y2  End point.
     */
    std::shared_ptr<Node> line(float x1, float y1, float x2, float y2);

    /*! @brief Creates a complex path node from a Path2D.
     *  @param[in] path  The path geometry.
     */
    std::shared_ptr<Node> path(const Path2D& path);

    /*! @brief Creates an empty group node for hierarchical composition. */
    std::shared_ptr<Node> group();

    // --- Root management ---

    /*! @brief Adds a node to the scene root list.
     *  @param[in] node  Node to add.
     */
    void addRoot(std::shared_ptr<Node> node);

    /*! @brief Removes a node from the scene root list.
     *  @param[in] node  Node to remove.
     */
    void removeRoot(const std::shared_ptr<Node>& node);

    /*! @brief Finds a node by its unique ID.
     *  @param[in] id  Node ID.
     *  @return Shared pointer to the node, or `nullptr` if not found.
     */
    std::shared_ptr<Node> findNode(uint32_t id) const;

    /*! @brief Returns a reference to the built-in Animator.
     *  @sa Animator
     */
    Animator& animator()
    {
        return animator_;
    }

    // --- Frame lifecycle ---

    /*! @brief Advances all animations by the given time step.
     *  @param[in] dt  Delta time in seconds.
     */
    void update(float dt);

    /*! @brief Renders the scene using the given renderer.
     *  @param[in] renderer   Renderer instance.
     *  @param[in] fbWidth    Framebuffer width.
     *  @param[in] fbHeight   Framebuffer height.
     */
    void render(Renderer& renderer, int fbWidth, int fbHeight);

    /*! @brief Renders the scene using the renderer supplied at construction. */
    void render();

    /*! @brief Returns the root node list. */
    const std::vector<std::shared_ptr<Node>>& roots() const
    {
        return roots_;
    }

    /*! @brief Collects all nodes into a flat list for rendering.
     *  @param[out] outList  Output vector populated with node pointers.
     */
    void collectRenderList(std::vector<Node*>& outList);

private:
    std::vector<std::shared_ptr<Node>> roots_;
    Animator animator_;
    Renderer* renderer_ = nullptr;

    void collectNodes(Node* node, std::vector<Node*>& outList);
};

} // namespace vectorgl
