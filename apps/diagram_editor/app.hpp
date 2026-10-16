#pragma once
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <chrono>
#include <optional>
#include <vectorgl/canvas.hpp>
#include <vectorgl/text_box.hpp>

#include "document.hpp"
#include "workspace.hpp"

namespace flow
{
struct Rect
{
    float x, y, w, h;
    bool contains(vectorgl::Vec2 p) const
    {
        return p.x >= x && p.y >= y && p.x <= x + w && p.y <= y + h;
    }
};
class App
{
public:
    explicit App(GLFWwindow* window, std::string font, std::filesystem::path stateDirectory = {},
                 std::filesystem::path startupDocument = {});
    void render(float width, float height, float dpi, float dt);
    void pointer(float x, float y);
    void mouse(int button, int action, int modifiers);
    void key(int key, int action, int modifiers);
    void character(uint32_t cp);
    void scroll(float amount);
    void requestClose();
    void open(const std::filesystem::path& path);
    void smokeTest(const std::filesystem::path& directory);
    void tickWorkspace(bool force = false);
    vectorgl::Canvas canvas;

private:
    enum class Action
    {
        None,
        New,
        Open,
        Save,
        SaveAs,
        Export,
        Undo,
        Redo,
        Add,
        Delete,
        Duplicate,
        Copy,
        Cut,
        Paste,
        Arrange,
        Fit,
        ZoomIn,
        ZoomOut,
        Grid,
        Snap,
        Theme,
        Select,
        Color,
        Kind,
        Rename,
        Help,
        Quit,
        ModalConfirm,
        ModalCancel,
        Discard,
        Recent,
        RecentOpen,
        RecentLoad,
        ClearRecent,
        RecoveryMenu,
        RestoreRecovery,
        ApplyRecovery,
        DiscardRecovery
    };
    enum class Modal
    {
        None,
        Unsaved,
        PathOpen,
        PathSave,
        PathExport,
        Rename,
        Help,
        Error,
        Recent,
        Recovery
    };
    struct Hit
    {
        Rect rect;
        Action action;
        uint32_t value = 0;
    };
    GLFWwindow* window_;
    std::string font_;
    Editor editor_;
    std::filesystem::path path_;
    Document savedDocument_;
    std::unique_ptr<Workspace> workspace_;
    std::optional<Document> checkpointDocument_;
    std::optional<Document> checkpointBaseline_;
    ViewState checkpointView_;
    std::filesystem::path checkpointSource_;
    std::chrono::steady_clock::time_point lastWorkspaceWrite_;
    std::string recoveryStatus_ = "Recovery ready";
    std::string status_ = "Ready. Drag cards to arrange your ideas.", error_;
    std::vector<uint32_t> selection_;
    std::string pastePayload_;
    // The GUI smoke test injects text so it never replaces the user's clipboard.
    std::optional<std::string> clipboardSimulation_;
    vectorgl::Vec2 pasteAnchor_{};
    uint32_t pasteCount_ = 0;
    std::optional<Document> nudgeBefore_;
    int nudgeKey_ = -1;
    uint32_t selectedEdge_ = 0, connecting_ = 0, inspected_ = 0;
    vectorgl::Vec2 cursor_{}, last_{}, start_{}, pan_{}, dragWorld_{};
    float zoom_ = 1, width_ = 1440, height_ = 900, layerScroll_ = 0;
    bool dark_ = false, grid_ = true, snap_ = true, panning_ = false, dragging_ = false, marquee_ = false;
    bool fieldDragging_ = false;
    std::optional<Document> dragBefore_;
    vectorgl::TextBox title_, description_, modalField_;
    Modal modal_ = Modal::None;
    Action pending_ = Action::None;
    uint32_t pendingValue_ = 0;
    std::vector<Hit> hits_;
    Rect board() const;
    vectorgl::Vec2 world(vectorgl::Vec2 p) const;
    vectorgl::Vec2 screen(vectorgl::Vec2 p) const;
    bool dirty() const;
    void fit();
    void syncFields();
    void commitFields();
    void finishGesture();
    void finishNudge();
    void action(Action action, uint32_t value = 0);
    bool save(bool choose);
    void fileAction(Action action);
    void guard(Action action, uint32_t value = 0);
    void rememberFile();
    void captureSettings();
    void completePending();
    void setError(const std::string& message);
    void text(const std::string& value, float x, float y, float size, vectorgl::Color color, float maxWidth = 0);
    void box(Rect rect, vectorgl::Color fill, float radius = 0);
    void outline(Rect rect, vectorgl::Color color, float radius = 0, float width = 1);
    void button(Rect rect, const std::string& label, Action action, uint32_t value = 0, bool primary = false,
                bool enabled = true);
    void connection(vectorgl::Vec2 from, vectorgl::Vec2 to, vectorgl::Color color, float width);
    void drawBoard();
    void drawChrome(float dt);
    void drawModal(float dt);
    void themeFields(vectorgl::TextBox& field);
};
} // namespace flow
