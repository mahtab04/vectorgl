#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <vectorgl/canvas.hpp>
#include <vectorgl/scene.hpp>
#include <vectorgl/ui/containers.hpp>
#include <vectorgl/ui/file_dialogs.hpp>
#include <vectorgl/ui/glfw_adapter.hpp>
#ifdef VECTORGL_HAS_NATIVE_DIALOGS
#include <vectorgl/ui/native_dialogs.hpp>
#endif
#include <vectorgl/ui/overlays.hpp>
#include <vectorgl/ui/table_view.hpp>
#include <vectorgl/ui/tree_view.hpp>
#include <vectorgl/ui/workspace_controls.hpp>
using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void screenshot(const std::string& path, int width, int height)
{
    std::vector<unsigned char> pixels(size_t(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for (size_t i = 0; i < pixels.size(); i += 4)
        std::swap(pixels[i], pixels[i + 2]);
    unsigned char header[54]{};
    auto u32 = [&](int offset, uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
            header[offset + i] = static_cast<unsigned char>(value >> (i * 8));
    };
    header[0] = 'B';
    header[1] = 'M';
    u32(2, static_cast<uint32_t>(54 + pixels.size()));
    u32(10, 54);
    u32(14, 40);
    u32(18, width);
    u32(22, height);
    header[26] = 1;
    header[28] = 32;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    if (!file)
        throw std::runtime_error("Could not write screenshot");
}

struct ToolboxSymbol
{
    ItemId id;
    int kind;
};
class WorkspaceFileBackend final : public FileDialogBackend
{
public:
    FileDialogResult result{{ServiceStatus::Success, {}}, "/example/selected.svg"};
    FileDialogCapabilities capabilities() const override
    {
        return {true, true, false, "Workspace fake"};
    }
    FileDialogResult show(const FileDialogOptions&) override
    {
        return result;
    }
};
class WorkspaceClipboard final : public Clipboard
{
    std::string text_;

public:
    bool failWrites = false;
    ClipboardResult read() override
    {
        return {{ServiceStatus::Success, {}}, text_};
    }
    ServiceResult write(std::string_view text) override
    {
        if (failWrites)
            return {ServiceStatus::Error, "Injected clipboard failure"};
        text_ = text;
        return {ServiceStatus::Success, {}};
    }
};
class EditorWorkspace
{
public:
    Canvas canvas;
    Context ui;
    GlfwAdapter host;
    OverlayManager overlays;
    std::exception_ptr error;
    EditorWorkspace(GLFWwindow* window, std::string font, bool smoke)
        : ui(Services{{},
                      smoke ? std::shared_ptr<Clipboard>(std::make_shared<WorkspaceClipboard>())
                            : std::make_shared<GlfwClipboard>(window),
                      {}}),
          host(window, ui), overlays(ui), scene_(canvas.renderer())
    {
        canvas.init();
        std::shared_ptr<FileDialogBackend> backend;
        if (smoke)
        {
            fakeFiles_ = std::make_shared<WorkspaceFileBackend>();
            backend = fakeFiles_;
        }
#ifdef VECTORGL_HAS_NATIVE_DIALOGS
        else
        {
            NativeDialogOptions native;
            native.ownerWindow = host.nativeDialogOwner();
            backend = createNativeFileDialogs(native);
        }
#endif
        files_ = std::make_unique<FileDialogService>(ui, std::move(backend));
        files_->setHostEventDrain([] { glfwPollEvents(); });
        auto theme = ui.theme();
        theme.fontPath = std::move(font);
        theme.fontSize = 16;
        ui.setTheme(theme);
        Widget root;
        root.layout = std::make_shared<ColumnLayout>(Padding{16, 16, 16, 16}, 10.0f);
        root.clipChildren = true;
        root_ = ui.add(root);
        label(root_, "VectorGL / standalone editor workspace", 1.45f);
        Widget bar;
        bar.parent = root_;
        bar.layout = std::make_shared<RowLayout>(Padding{}, 10.0f);
        const auto toolbar = ui.add(bar);
        LayoutHints toolbarHints;
        toolbarHints.height = 40.0f;
        ui.setLayoutHints(toolbar, toolbarHints);
        button(toolbar, "Open...", [this] { requestFile(FileDialogKind::Open); });
        button(toolbar, "Save / close", [this] { showDialog(); });
        button(toolbar, "Zoom -", [this] { setZoom(zoom_ * .9f); });
        button(toolbar, "Zoom +", [this] { setZoom(zoom_ / .9f); });
        button(toolbar, "Place selected",
               [this]
               {
                   if (toolbox_->currentItem())
                       place({*toolbox_->currentItem(),
                              *toolbox_->currentItem() == 32 ? 3 : int((*toolbox_->currentItem() - 1) % 3)},
                             {100, 100});
               });
        label(toolbar, "Drag toolbox to artboard | Enter places | Ctrl+wheel: artboard zoom", .85f);
        SplitOptions left;
        left.fraction = .19f;
        left.firstMinimum = 210;
        left.secondMinimum = 760;
        left.firstMaximum = 400;
        outer_ = std::make_unique<SplitPanel>(ui, root_, left);
        flex(outer_->id());
        column(outer_->second());
        SplitOptions right;
        right.fraction = .72f;
        right.firstMinimum = 480;
        right.secondMinimum = 260;
        right.secondMaximum = 420;
        middle_ = std::make_unique<SplitPanel>(ui, outer_->second(), right);
        flex(middle_->id());
        column(middle_->first());
        SplitOptions bottom;
        bottom.axis = SplitAxis::Vertical;
        bottom.fraction = .64f;
        bottom.firstMinimum = 230;
        bottom.secondMinimum = 220;
        bottom_ = std::make_unique<SplitPanel>(ui, middle_->first(), bottom);
        flex(bottom_->id());
        buildToolbox(outer_->first());
        buildArtboard(bottom_->first());
        buildInspector(middle_->second());
        buildTable(bottom_->second());
        status_ = std::make_unique<Label>(ui, canvas, root_, LabelOptions{"Ready / typed owned payloads / no Qt"});
        sceneGroup_ = scene_.group();
        place({1, 0}, {80, 80});
        place({2, 1}, {320, 170});
        dirty_ = false;
    }
    ~EditorWorkspace()
    {
        try
        {
            ui.remove(root_);
        }
        catch (...)
        {
        }
    }
    void prepare(const Viewport& viewport)
    {
        viewport_ = viewport;
        ui.layout(root_, {0, 0, viewport.width(), viewport.height()});
        overlays.update({0, 0, viewport.width(), viewport.height()});
        syncSceneTransform();
    }
    template <class F> void callback(F&& f) noexcept
    {
        if (error)
            return;
        try
        {
            host.refreshMetrics();
            if (auto v = host.metrics().viewport())
                prepare(*v);
            f();
            if (auto v = host.metrics().viewport())
                prepare(*v);
        }
        catch (...)
        {
            error = std::current_exception();
        }
    }
    void recordFrame()
    {
        ++paintedFrames_;
        lastStats_ = canvas.renderer().frameStats();
        if (frameStarted_)
        {
            submitMs_ +=
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStart_).count();
            frameStarted_ = false;
        }
    }
    void beginFrameMeasurement()
    {
        frameStart_ = std::chrono::steady_clock::now();
        frameStarted_ = true;
    }
    void pumpPlatformServices()
    {
        if (files_->pending())
            files_->pump();
    }
    bool platformPending() const
    {
        return files_->pending();
    }
    void command(int key)
    {
        if (key != GLFW_KEY_DELETE || !selected_ || ui.focused() != drawing_)
            return;
        sceneGroup_->removeChild(selected_);
        std::erase_if(placed_, [this](const Placed& placed) { return placed.node == selected_; });
        selected_.reset();
        refreshTree();
        dirty_ = true;
        ui.invalidate();
    }
    void smokeTest()
    {
        const auto native = host.metrics().viewport().value();
        const auto sourcePoint = [&](std::size_t row)
        {
            const auto b = ui.bounds(toolbox_->viewportId()).value();
            return ui
                .mapToRoot(toolbox_->viewportId(), {b.x + 30, b.y + float(row) * 36 + 18 - toolbox_->scrollOffset()})
                .value();
        };
        const auto dragTo = [&](std::size_t row, Vec2 target, bool cancel, bool loseFocus = false)
        {
            const auto source = sourcePoint(row);
            Event e;
            e.type = EventType::PointerDown;
            e.x = source.x;
            e.y = source.y;
            e.button = PointerButton::Primary;
            ui.dispatch(e);
            e.type = EventType::PointerMove;
            e.x += 9;
            e.button = PointerButton::None;
            ui.dispatch(e);
            require(ui.dragState().has_value(), "Workspace toolbox did not start typed drag");
            e.x = target.x;
            e.y = target.y;
            ui.dispatch(e);
            if (cancel)
            {
                if (loseFocus)
                    host.focusChanged(false);
                else
                {
                    Event escape;
                    escape.type = EventType::KeyDown;
                    escape.key = Key::Escape;
                    ui.dispatch(escape);
                }
            }
            else
            {
                e.type = EventType::PointerUp;
                e.button = PointerButton::Primary;
                ui.dispatch(e);
            }
            require(!ui.dragState() && !ui.captured(), "Workspace drag did not settle");
        };
        const auto toolboxBounds = ui.bounds(toolbox_->id());
        setZoom(1.25f);
        prepare(native);
        require(ui.bounds(toolbox_->id()) == toolboxBounds, "Artboard zoom changed UI layout");
        const auto target = ui.mapToRoot(drawing_, {140 * zoom_, 120 * zoom_}).value();
        const auto before = placed_.size();
        dragTo(0, target, false);
        require(placed_.size() == before + 1, "Accepted drop did not create scene node");
        require(std::abs(placed_.back().node->position().x - 200) < .01f, "Drop did not invert artboard zoom");
        dragTo(0, {native.width() - 10, native.height() - 10}, false);
        dragTo(0, target, true);
        dragTo(0, target, true, true);
        require(placed_.size() == before + 1, "Rejected/cancelled drop modified scene");
        // Explicitly rejected typed symbol, independent of source row visibility.
        toolbox_->setSelection({32});
        toolbox_->scrollToItem(32);
        prepare(native);
        dragTo(31, target, false);
        require(placed_.size() == before + 1, "Unsupported symbol drop was accepted");
        toolbox_->setScrollOffset(0);
        setZoom(1);
        prepare(native);
        tags_->setSelection({1500});
        tagSearch_->setQuery("tag_0001");
        filterTags();
        require(tags_->selection() == std::vector<ItemId>{1500}, "Filtering discarded hidden tag selection");
        tagSearch_->setQuery("");
        filterTags();
        tags_->setSort(TableSort{2, SortDirection::Descending});
        filterTags();
        prepare(native);
        ui.focus(tags_->bodyId());
        host.key(GLFW_KEY_END, GLFW_PRESS, 0);
        require(tags_->currentItem() == 1, "Tag reorder/navigation failed");
        tree_->setSelection({2});
        tree_->setExpanded(1, false);
        require(tree_->selection() == std::vector<ItemId>{2}, "Tree collapse discarded hidden selection");
        tree_->setExpanded(1, true);
        name_->setText("Property text");
        ui.focus(name_->id());
        require(host.key(GLFW_KEY_DELETE, GLFW_PRESS, 0), "Inspector Delete escaped to application commands");
        require(placed_.size() == before + 1, "Inspector Delete reached artboard commands");
        const auto clipboard = std::dynamic_pointer_cast<WorkspaceClipboard>(ui.services().clipboard);
        require(bool(clipboard), "Smoke mode must not use the OS clipboard");
        host.key(GLFW_KEY_A, GLFW_PRESS, GLFW_MOD_CONTROL);
        const auto retainedText = name_->text();
        clipboard->failWrites = true;
        host.key(GLFW_KEY_X, GLFW_PRESS, GLFW_MOD_CONTROL);
        require(name_->text() == retainedText, "Failed clipboard cut removed property text");
        clipboard->failWrites = false;
        host.key(GLFW_KEY_TAB, GLFW_PRESS, 0);
        require(ui.focused() != name_->id(), "Workspace Tab traversal failed");
        showDialog();
        overlays.update({0, 0, native.width(), native.height()});
        require(bool(ui.modalRoot()), "Save/Discard/Cancel dialog was not modal");
        Event escape;
        escape.type = EventType::KeyDown;
        escape.key = Key::Escape;
        ui.dispatch(escape);
        overlays.update({0, 0, native.width(), native.height()});
        require(!ui.modalRoot(), "Dialog Escape did not cancel");
        ui.focus({});
        ui.dispatch(Event{EventType::WindowFocusLost});
        // Public API rendering into embedded nonzero FBOs at four DPI scales,
        // including a nonuniform X/Y ratio and active field/popup cases.
        testScales(native);
        for (auto serviceStatus :
             {ServiceStatus::Success, ServiceStatus::Cancelled, ServiceStatus::Error, ServiceStatus::Unavailable})
        {
            fakeFiles_->result = {
                {serviceStatus, serviceStatus == ServiceStatus::Error ? "Injected backend failure" : ""},
                "/example/selected.svg"};
            requestFile(FileDialogKind::Open);
            pumpPlatformServices();
            require(lastFile_.result.status == serviceStatus && !ui.modalRoot(),
                    "Workspace dialog result/cleanup failed");
        }
        fakeFiles_->result = {{ServiceStatus::Success, {}}, "/example/workspace.svg"};
        requestFile(FileDialogKind::Save);
        pumpPlatformServices();
        require(lastFile_.result.status == ServiceStatus::Success, "Workspace save selection failed");
        ui.focus({});
        prepare(native);
        ui.takeInvalidation();
        prepare(native);
        require(ui.pendingInvalidation() == Invalidation::None, "Idle workspace repeatedly invalidated");
        require(!ui.nextWakeup(), "Unfocused idle workspace retained wakeup timer");
        const double cpuBefore = processCpuSeconds();
        const auto idleStart = std::chrono::steady_clock::now();
        while (std::chrono::duration<double>(std::chrono::steady_clock::now() - idleStart).count() < .05)
            glfwWaitEventsTimeout(.01);
        const double idleWall =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - idleStart).count();
        const double idleCpu = (processCpuSeconds() - cpuBefore) * 1000;
        std::cout << "workspace: " << native.width() << "x" << native.height() << " / visible tag rows "
                  << tags_->visibleRows().end - tags_->visibleRows().first << " / draw calls " << lastStats_.drawCalls
                  << " / upload bytes " << lastStats_.bufferUploadBytes << " / frames " << paintedFrames_
                  << " / mean submit_ms " << submitMs_ / double(paintedFrames_) << " / idle_cpu_ms " << idleCpu
                  << " / idle_wall_ms " << idleWall << " / renderer "
                  << reinterpret_cast<const char*>(glGetString(GL_RENDERER)) << '\n';
    }

private:
    struct Placed
    {
        ItemId id;
        ToolboxSymbol symbol;
        std::shared_ptr<Node> node;
        std::string name;
    };
    WidgetId root_, drawing_;
    std::optional<Viewport> viewport_;
    Scene scene_;
    std::shared_ptr<Node> sceneGroup_, selected_;
    std::vector<Placed> placed_;
    float zoom_ = 1;
    bool dirty_ = false;
    ItemId nextPlaced_ = 2;
    std::optional<std::pair<Vec2, ItemId>> press_;
    std::unique_ptr<SplitPanel> outer_, middle_, bottom_;
    std::unique_ptr<SearchField> search_, tagSearch_;
    std::unique_ptr<ListView> toolbox_;
    std::unique_ptr<ScrollView> artboard_, inspector_;
    std::unique_ptr<TabView> inspectorTabs_;
    std::unique_ptr<TreeView> tree_;
    std::unique_ptr<TableView> tags_;
    std::unique_ptr<TextField> name_;
    std::unique_ptr<NumericEditor> opacity_;
    std::unique_ptr<ComboBox> choice_;
    std::shared_ptr<WorkspaceFileBackend> fakeFiles_;
    std::unique_ptr<FileDialogService> files_;
    FileDialogResult lastFile_;
    std::unique_ptr<CheckBox> enabled_;
    std::unique_ptr<Label> status_;
    std::vector<std::unique_ptr<Label>> labels_;
    std::vector<std::unique_ptr<Button>> buttons_;
    std::vector<std::unique_ptr<CollapsibleSection>> sections_;
    std::uint64_t paintedFrames_ = 0;
    Renderer::FrameStats lastStats_;
    std::chrono::steady_clock::time_point frameStart_;
    bool frameStarted_ = false;
    double submitMs_ = 0;
    static double processCpuSeconds()
    {
#ifdef _WIN32
        FILETIME created, exited, kernel, user;
        if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
            throw std::runtime_error("CPU measurement unavailable");
        ULARGE_INTEGER k{}, u{};
        k.LowPart = kernel.dwLowDateTime;
        k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime;
        u.HighPart = user.dwHighDateTime;
        return (double(k.QuadPart) + double(u.QuadPart)) * 1e-7;
#else
        const auto ticks = std::clock();
        if (ticks == std::clock_t(-1))
            throw std::runtime_error("CPU measurement unavailable");
        return double(ticks) / CLOCKS_PER_SEC;
#endif
    }
    static void require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    void column(WidgetId id)
    {
        ui.setLayout(id, std::make_shared<ColumnLayout>(Padding{6, 6, 6, 6}, 8.0f));
    }
    void flex(WidgetId id)
    {
        LayoutHints h;
        h.flex = 1;
        ui.setLayoutHints(id, h);
    }
    void label(WidgetId parent, std::string text, float scale = 1)
    {
        LabelOptions o;
        o.text = std::move(text);
        o.fontScale = scale;
        labels_.push_back(std::make_unique<Label>(ui, canvas, parent, o));
        LayoutHints hints;
        hints.height = std::ceil(26.0f * scale);
        ui.setLayoutHints(labels_.back()->id(), hints);
    }
    void button(WidgetId parent, std::string text, std::function<void()> action)
    {
        buttons_.push_back(
            std::make_unique<Button>(ui, parent, ButtonOptions{std::move(text), {140, 36}, std::move(action)}));
    }
    void status(std::string text)
    {
        if (status_ && status_->text() != text)
            status_->setText(std::move(text));
    }
    static std::string folded(std::string text)
    {
        for (auto& c : text)
            if (c >= 'A' && c <= 'Z')
                c = char(c - 'A' + 'a');
        return text;
    }
    void buildToolbox(WidgetId parent)
    {
        column(parent);
        label(parent, "Toolbox", 1.15f);
        SearchFieldOptions search;
        search.placeholder = "Find symbols";
        search.onChanged = [this](std::string_view) { filterTools(); };
        search_ = std::make_unique<SearchField>(ui, canvas, parent, search);
        LayoutHints searchHints;
        searchHints.height = 44.0f;
        ui.setLayoutHints(search_->id(), searchHints);
        ListViewOptions list;
        list.onActivated = [this](ItemId id) { place({id, id == 32 ? 3 : int((id - 1) % 3)}, {120, 120}); };
        list.eventFilter = [this](const Event& e) -> std::optional<EventReply>
        {
            if (e.type == EventType::Cancel)
                press_.reset();
            if (e.type == EventType::PointerDown && e.button == PointerButton::Primary)
            {
                const auto b = ui.bounds(toolbox_->viewportId());
                const auto p = ui.mapFromRoot(toolbox_->viewportId(), {e.x, e.y});
                if (b && p && b->contains(p->x, p->y))
                {
                    const auto row = std::size_t((p->y - b->y + toolbox_->scrollOffset()) / 36);
                    if (row < toolbox_->model()->rowCount())
                        press_ = std::make_pair(Vec2{e.x, e.y}, toolbox_->model()->itemId(row));
                }
            }
            if (e.type == EventType::PointerMove && press_ && ui.captured() == toolbox_->id() &&
                std::hypot(e.x - press_->first.x, e.y - press_->first.y) >= 6)
            {
                const auto id = press_->second;
                ui.beginDrag(toolbox_->id(), DragPayload::make(ToolboxSymbol{id, id == 32 ? 3 : int((id - 1) % 3)}),
                             [this](const DragResult& result)
                             {
                                 status(result.outcome == DragOutcome::Dropped    ? "Drop accepted"
                                        : result.outcome == DragOutcome::Rejected ? "Drop rejected"
                                                                                  : "Drag cancelled");
                             });
                press_.reset();
                return EventReply{true};
            }
            if (e.type == EventType::PointerUp)
                press_.reset();
            return {};
        };
        toolbox_ = std::make_unique<ListView>(ui, parent, nullptr, list);
        flex(toolbox_->id());
        filterTools();
        label(parent, "Last item demonstrates rejection", .8f);
    }
    void filterTools()
    {
        std::vector<ListItem> rows;
        std::vector<ItemId> universe;
        const auto query = folded(search_->query());
        for (ItemId id = 1; id <= 32; ++id)
        {
            universe.push_back(id);
            const auto name = id == 32 ? "Unsupported payload"
                                       : std::string(id % 3 == 1   ? "Panel "
                                                     : id % 3 == 2 ? "Indicator "
                                                                   : "Button ") +
                                             std::to_string(id);
            if (folded(name).find(query) != std::string::npos)
                rows.push_back({id, name});
        }
        toolbox_->setProjection(std::make_shared<VectorListModel>(std::move(rows)), std::move(universe));
    }
    struct Board final : WidgetPainter
    {
        EditorWorkspace& owner;
        explicit Board(EditorWorkspace& app) : owner(app) {}
        void paint(PaintContext& p) override
        {
            owner.paintBoard(p);
        }
    };
    void buildArtboard(WidgetId parent)
    {
        column(parent);
        label(parent, "Artboard / separate Scene + zoom", 1.1f);
        ScrollOptions scroll;
        scroll.horizontal = true;
        artboard_ = std::make_unique<ScrollView>(ui, parent, scroll);
        flex(artboard_->id());
        artboard_->setContentSize({1000, 700});
        Widget drawing;
        drawing.parent = artboard_->content();
        drawing.bounds = {0, 0, 1000, 700};
        drawing.focusable = true;
        drawing.painter = std::make_shared<Board>(*this);
        drawing.onDrag = [this](Context&, WidgetId, const DragEvent& event)
        {
            const auto* symbol = event.payload.get<ToolboxSymbol>();
            const bool accepted = symbol && symbol->kind < 3 && event.localX >= 0 && event.localY >= 0 &&
                                  event.localX < 1000 * zoom_ && event.localY < 700 * zoom_;
            if (event.phase == DragPhase::Drop && accepted)
                place(*symbol, {event.localX / zoom_, event.localY / zoom_});
            return accepted;
        };
        drawing.onEvent = [this](Context&, WidgetId, const Event& e)
        {
            if (e.type == EventType::Wheel && e.modifiers.control)
            {
                setZoom(zoom_ * (e.wheelY > 0 ? 1.1f : .9f));
                return EventReply{true};
            }
            if (e.type == EventType::PointerDown)
            {
                if (name_ && name_->editing())
                    name_->commit();
                if (opacity_ && opacity_->editing())
                    opacity_->commit();
                const auto root = ui.mapToRoot(drawing_, {e.localX, e.localY});
                if (root && viewport_)
                    selected_ = scene_.pick(root->x * viewport_->scaleX(), root->y * viewport_->scaleY());
                syncInspector();
                return EventReply{true, true};
            }
            return EventReply{};
        };
        drawing_ = ui.add(drawing);
    }
    void paintBoard(PaintContext& p)
    {
        p.fillRect(p.bounds(), p.theme().background);
        auto& c = p.canvas();
        c.setFillColor(p.theme().border);
        for (int y = 20; y < 700; ++y)
        {
            if (y % 40)
                continue;
            for (int x = 20; x < 1000; x += 40)
                c.fillCircle(float(x) * zoom_, float(y) * zoom_, 1);
        }
        if (sceneGroup_ && viewport_)
        {
            syncSceneTransform();
            c.renderer().flush();
            scene_.render();
            c.renderer().flush();
        }
        if (auto drag = ui.dragState())
        {
            const auto point = ui.mapFromRoot(drawing_, {drag->x, drag->y});
            if (point)
            {
                c.setStrokeColor(drag->accepted ? p.theme().accent : p.theme().error);
                c.setLineWidth(3);
                c.strokeRect(point->x - 40, point->y - 24, 80, 48);
            }
        }
    }
    void setZoom(float value)
    {
        zoom_ = std::clamp(value, .5f, 2.0f);
        artboard_->setContentSize({1000 * zoom_, 700 * zoom_});
        ui.setBounds(drawing_, {0, 0, 1000 * zoom_, 700 * zoom_});
        status("Artboard zoom: " + std::to_string(int(zoom_ * 100)) + "% / UI scale unchanged");
    }
    void syncSceneTransform()
    {
        if (!sceneGroup_ || !viewport_)
            return;
        const auto origin = ui.mapToRoot(drawing_, {});
        if (!origin)
            return;
        sceneGroup_->setPosition(origin->x * viewport_->scaleX(), origin->y * viewport_->scaleY());
        sceneGroup_->setScale(zoom_ * viewport_->scaleX(), zoom_ * viewport_->scaleY());
    }
    void place(ToolboxSymbol symbol, Vec2 point)
    {
        if (symbol.kind >= 3)
        {
            status("Unsupported symbol rejected");
            return;
        }
        std::shared_ptr<Node> node =
            symbol.kind == 1 ? scene_.circle(point.x, point.y, 32) : scene_.roundedRect(point.x, point.y, 120, 70, 10);
        node->setFill(symbol.kind == 1 ? Color::Amber : symbol.kind == 2 ? Color::Emerald : Color::CornflowerBlue);
        scene_.removeRoot(node);
        sceneGroup_->addChild(node);
        selected_ = node;
        const auto id = nextPlaced_++;
        placed_.push_back({id, symbol, node, "Symbol #" + std::to_string(id)});
        syncInspector();
        dirty_ = true;
        refreshTree();
        ui.invalidate();
    }
    void buildInspector(WidgetId parent)
    {
        column(parent);
        label(parent, "Inspector / hierarchy", 1.1f);
        inspectorTabs_ = std::make_unique<TabView>(ui, parent, TabBarOptions{{{"Properties"}, {"Hierarchy"}}, 0});
        flex(inspectorTabs_->id());
        inspector_ = std::make_unique<ScrollView>(ui, inspectorTabs_->page(0));
        flex(inspector_->id());
        inspector_->setContentLayout(std::make_shared<ColumnLayout>(Padding{6, 6, 6, 6}, 10.0f));
        auto section = std::make_unique<CollapsibleSection>(ui, inspector_->content(), SectionOptions{"Appearance"});
        const auto content = section->content();
        sections_.push_back(std::move(section));
        label(content, "Name");
        TextFieldOptions field;
        field.value = "Selected symbol";
        field.onEdit = [this](const EditEvent& e)
        {
            if (e.kind == EditKind::Committed)
            {
                for (auto& placed : placed_)
                    if (placed.node == selected_)
                        placed.name = e.text;
                refreshTree();
                dirty_ = true;
                status("Name committed: " + e.text);
            }
        };
        field.onServiceError = [this](const ServiceResult& r) { status("Clipboard error: " + r.message); };
        name_ = std::make_unique<TextField>(ui, canvas, content, field);
        label(content, "Opacity");
        NumericOptions opacity;
        opacity.value = 1;
        opacity.minimum = 0;
        opacity.maximum = 1;
        opacity.step = .05;
        opacity.onEdit = [this](const NumberEditEvent& e)
        {
            if (e.kind == EditKind::Committed && selected_)
            {
                selected_->setOpacity(float(e.value));
                dirty_ = true;
                ui.invalidate();
            }
        };
        opacity_ = std::make_unique<NumericEditor>(ui, canvas, content, opacity);
        label(content, "Symbolic state");
        ComboBoxOptions choices;
        choices.items = {{"Automatic"}, {"Manual"}, {"Maintenance"}};
        choices.selected = 0;
        choices.onChanged = [this](std::size_t id) { status("Symbolic state #" + std::to_string(id)); };
        choice_ = std::make_unique<ComboBox>(ui, overlays, content, choices);
        CheckBoxOptions enabled;
        enabled.label = "Enabled";
        enabled.checked = true;
        enabled.onChanged = [this](bool value)
        {
            if (selected_)
            {
                selected_->setVisible(value);
                dirty_ = true;
                ui.invalidate();
            }
        };
        enabled_ = std::make_unique<CheckBox>(ui, content, enabled);
        label(content, "Property edits consume editing keys", .8f);
        TreeViewOptions treeOptions;
        treeOptions.selectionMode = ListSelectionMode::Multiple;
        treeOptions.onActivated = [this](ItemId id)
        {
            for (const auto& placed : placed_)
                if (placed.id == id)
                {
                    selected_ = placed.node;
                    syncInspector();
                    ui.invalidate();
                }
        };
        tree_ = std::make_unique<TreeView>(ui, inspectorTabs_->page(1), nullptr, treeOptions);
        flex(tree_->id());
        refreshTree();
    }
    void refreshTree()
    {
        if (!tree_)
            return;
        std::vector<TreeItem> nodes{{1, 0, "Artboard"}};
        for (const auto& placed : placed_)
            nodes.push_back({placed.id, 1, placed.name});
        const bool initiallyEmpty = tree_->visibleItemCount() == 0 || placed_.size() == 1;
        tree_->setModel(std::make_shared<VectorTreeModel>(std::move(nodes)));
        if (initiallyEmpty)
            tree_->setExpanded(1, true);
    }
    void syncInspector()
    {
        if (!selected_ || !name_ || !opacity_ || !enabled_)
            return;
        for (const auto& placed : placed_)
            if (placed.node == selected_)
                name_->setText(placed.name);
        opacity_->setValue(selected_->style().opacity);
        enabled_->setChecked(selected_->visible());
    }
    void buildTable(WidgetId parent)
    {
        column(parent);
        label(parent, "Tag browser / 2,000 rows", 1.1f);
        SearchFieldOptions search;
        search.placeholder = "Filter tags";
        search.onChanged = [this](std::string_view) { filterTags(); };
        tagSearch_ = std::make_unique<SearchField>(ui, canvas, parent, search);
        LayoutHints searchHints;
        searchHints.height = 44.0f;
        ui.setLayoutHints(tagSearch_->id(), searchHints);
        TableViewOptions options;
        options.rowHeight = 30;
        options.selectionMode = ListSelectionMode::Multiple;
        options.onSortRequested = [this](TableSort) { filterTags(); };
        tags_ = std::make_unique<TableView>(
            ui, parent, nullptr,
            std::vector<TableColumn>{{1, "Tag", 240, 100, 600}, {2, "Value", 120, 60, 300}, {3, "State", 140, 80, 300}},
            options);
        flex(tags_->id());
        filterTags();
    }
    void filterTags()
    {
        std::vector<TableItem> rows;
        std::vector<ItemId> universe;
        const auto query = folded(tagSearch_->query());
        for (ItemId id = 1; id <= 2000; ++id)
        {
            universe.push_back(id);
            const auto number = std::to_string(id);
            const auto tag = "TAG_" + std::string(4 - number.size(), '0') + number;
            if (folded(tag).find(query) != std::string::npos)
                rows.push_back({id, {{1, tag}, {2, number}, {3, id % 13 ? "Ready" : "Alarm"}}});
        }
        if (tags_->sort() && tags_->sort()->direction == SortDirection::Descending)
            std::reverse(rows.begin(), rows.end());
        tags_->setProjection(std::make_shared<VectorTableModel>(std::move(rows)), std::move(universe));
    }
    void showDialog()
    {
        DialogOptions dialog;
        dialog.title = dirty_ ? "Unsaved workspace changes" : "Workspace actions";
        dialog.message =
            "Save, discard or cancel. This sample reports the result; application persistence is separate.";
        dialog.actions = {{"Save", 1}, {"Discard", 2}, {"Cancel", 0}};
        dialog.defaultAction = 0;
        dialog.cancelAction = 2;
        dialog.onFinished = [this](std::optional<int> result)
        {
            if (result && *result == 2)
                dirty_ = false;
            if (result && *result == 1)
            {
                requestFile(FileDialogKind::Save);
                return;
            }
            status(!result || *result == 0 ? "Close cancelled"
                   : *result == 1          ? "Save requested (connect application persistence)"
                                           : "Discard selected");
        };
        overlays.openDialog(dialog);
    }
    void requestFile(FileDialogKind kind)
    {
        FileDialogOptions options;
        options.kind = kind;
        options.title = kind == FileDialogKind::Open ? "Open a VectorGL document" : "Choose save destination";
        if (kind == FileDialogKind::Save)
            options.suggestedName = "workspace.svg";
        options.filters = {{"VectorGL documents", {"svg", "json"}}, {"All files", {}}};
        files_->request(
            std::move(options),
            [this, kind](const FileDialogResult& result)
            {
                lastFile_ = result;
                if (result.result.status == ServiceStatus::Success)
                    status(
                        std::string(kind == FileDialogKind::Open ? "Open selected: " : "Save destination selected: ") +
                        result.path + " / connect application serialization");
                else
                    status(result.result.status == ServiceStatus::Cancelled ? "File selection cancelled"
                           : result.result.status == ServiceStatus::Unavailable
                               ? "File dialogs unavailable: " + result.result.message
                               : "File dialog error: " + result.result.message);
            },
            root_);
    }
    struct TestFramebuffer
    {
        GLint draw = 0, read = 0;
        std::array<GLint, 4> viewport{};
        GLuint fbo = 0, color = 0, depth = 0;
        TestFramebuffer()
        {
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
            glGetIntegerv(GL_VIEWPORT, viewport.data());
        }
        ~TestFramebuffer()
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(draw));
            glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(read));
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            if (fbo)
                glDeleteFramebuffers(1, &fbo);
            if (color)
                glDeleteTextures(1, &color);
            if (depth)
                glDeleteRenderbuffers(1, &depth);
        }
        void allocate(int width, int height)
        {
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glGenTextures(1, &color);
            glBindTexture(GL_TEXTURE_2D, color);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
            glGenRenderbuffers(1, &depth);
            glBindRenderbuffer(GL_RENDERBUFFER, depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
            require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                    "Workspace test FBO incomplete");
        }
    };
    void testScales(const Viewport& native)
    {
        for (float scale : {1.0f, 1.25f, 1.5f, 2.0f})
        {
            TestFramebuffer target;
            const int fw = int(native.width() * scale), fh = int(native.height() * scale);
            target.allocate(fw, fh);
            Viewport view(native.width(), native.height(), fw, fh);
            prepare(view);
            if (scale == 1)
                ui.focus(name_->id());
            glClearColor(0, 0, 0, 1);
            beginFrameMeasurement();
            glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            canvas.beginFrame(fw, fh);
            ui.paint(canvas, view);
            canvas.endFrame();
            recordFrame();
            GLint binding = 0;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &binding);
            require(GLuint(binding) == target.fbo, "Workspace painting changed embedded framebuffer");
            const auto shape = ui.mapToRoot(drawing_, {140 * zoom_, 115 * zoom_}).value();
            std::array<unsigned char, 4> pixel{};
            glReadPixels(int(shape.x * view.scaleX()), fh - 1 - int(shape.y * view.scaleY()), 1, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixel.data());
            require(std::abs(int(pixel[0]) - 100) < 12 && std::abs(int(pixel[1]) - 149) < 12 &&
                        std::abs(int(pixel[2]) - 237) < 12,
                    "Scene pixels disagree with workspace transform/DPI");
            WindowMetrics metrics(int(native.width()), int(native.height()), fw, fh, scale, scale);
            const auto p = metrics.logicalPoint(100, 100);
            require(p && std::abs(p->x - 100) < 1 && std::abs(p->y - 100) < 1, "DPI cursor mapping mismatch");
            if (scale == 1.25f)
            {
                choice_->open();
                overlays.update({0, 0, view.width(), view.height()});
            }
        }
        overlays.closeAll();
        overlays.update({0, 0, native.width(), native.height()});
        TestFramebuffer target;
        const int fw = int(native.width() * 1.25f), fh = int(native.height() * 1.5f);
        target.allocate(fw, fh);
        Viewport view(native.width(), native.height(), fw, fh);
        prepare(view);
        beginFrameMeasurement();
        canvas.beginFrame(fw, fh);
        ui.paint(canvas, view);
        canvas.endFrame();
        recordFrame();
        WindowMetrics metrics(int(native.width()), int(native.height()), fw, fh, 1.25f, 1.5f);
        const auto mapped = metrics.logicalPoint(100, 100);
        require(mapped && std::abs(mapped->x - 100) < 1 && std::abs(mapped->y - 100) < 1,
                "Nonuniform cursor mapping mismatch");
        require(glGetError() == GL_NO_ERROR, "Workspace DPI/embedded-FBO OpenGL error");
    }
};
} // namespace
int main(int argc, char** argv)
{
    bool smoke = false;
    std::string font, capture;
    int width = 1680, height = 980;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--width" && i + 1 < argc)
            width = std::stoi(argv[++i]);
        else if (arg == "--height" && i + 1 < argc)
            height = std::stoi(argv[++i]);
        else if (arg == "--smoke-test")
            smoke = true;
        else if (arg == "--font" && i + 1 < argc)
            font = argv[++i];
        else if (arg == "--screenshot" && i + 1 < argc)
            capture = argv[++i];
    }
    if (font.empty())
        for (auto path : {"C:/Windows/Fonts/segoeui.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"})
            if (std::filesystem::exists(path))
            {
                font = path;
                break;
            }
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, smoke ? GLFW_FALSE : GLFW_TRUE);
    auto* window = glfwCreateWindow(width, height, "VectorGL - standalone editor workspace", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    int result = 0;
    try
    {
        if (!gladLoadGL(glfwGetProcAddress))
            throw std::runtime_error("GLAD initialization failed");
        EditorWorkspace demo(window, font, smoke);
        glfwSetWindowUserPointer(window, &demo);
        struct CallbackScope
        {
            GLFWwindow* window;
            ~CallbackScope()
            {
                glfwSetCursorPosCallback(window, nullptr);
                glfwSetMouseButtonCallback(window, nullptr);
                glfwSetScrollCallback(window, nullptr);
                glfwSetKeyCallback(window, nullptr);
                glfwSetCharCallback(window, nullptr);
                glfwSetWindowFocusCallback(window, nullptr);
                glfwSetWindowIconifyCallback(window, nullptr);
                glfwSetCursorEnterCallback(window, nullptr);
                glfwSetWindowSizeCallback(window, nullptr);
                glfwSetFramebufferSizeCallback(window, nullptr);
                glfwSetWindowContentScaleCallback(window, nullptr);
                glfwSetWindowRefreshCallback(window, nullptr);
                glfwSetWindowUserPointer(window, nullptr);
            }
        } callbacks{window};
        // Callbacks belong to this example, not the adapter. Catch at every native boundary.
        glfwSetCursorPosCallback(window,
                                 [](GLFWwindow* w, double x, double y)
                                 {
                                     auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                     d.callback([&] { d.host.cursorPosition(x, y); });
                                 });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int b, int a, int m)
                                   {
                                       auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.mouseButton(b, a, m); });
                                   });
        glfwSetScrollCallback(window,
                              [](GLFWwindow* w, double x, double y)
                              {
                                  auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                  d.callback([&] { d.host.scroll(x, y); });
                              });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int k, int, int a, int m)
                           {
                               auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                               d.callback(
                                   [&]
                                   {
                                       if (!d.host.key(k, a, m) && a == GLFW_PRESS)
                                       {
                                           if (k == GLFW_KEY_ESCAPE)
                                               glfwSetWindowShouldClose(w, true);
                                           else
                                               d.command(k);
                                       }
                                   });
                           });
        glfwSetCharCallback(window,
                            [](GLFWwindow* w, unsigned int c)
                            {
                                auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                d.callback([&] { d.host.character(c); });
                            });
        glfwSetWindowFocusCallback(window,
                                   [](GLFWwindow* w, int f)
                                   {
                                       auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.focusChanged(f != 0); });
                                   });
        glfwSetCursorEnterCallback(window,
                                   [](GLFWwindow* w, int e)
                                   {
                                       auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.cursorEntered(e != 0); });
                                   });
        glfwSetWindowIconifyCallback(window,
                                     [](GLFWwindow* w, int iconified)
                                     {
                                         auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                         d.callback([&] { d.host.iconified(iconified != 0); });
                                     });
        glfwSetWindowSizeCallback(window,
                                  [](GLFWwindow* w, int, int)
                                  {
                                      auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                      d.callback([&] { d.host.refreshMetrics(); });
                                  });
        glfwSetFramebufferSizeCallback(window,
                                       [](GLFWwindow* w, int, int)
                                       {
                                           auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                           d.callback([&] { d.host.refreshMetrics(); });
                                       });
        glfwSetWindowContentScaleCallback(window,
                                          [](GLFWwindow* w, float, float)
                                          {
                                              auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                              d.callback([&] { d.host.refreshMetrics(); });
                                          });
        glfwSetWindowRefreshCallback(window,
                                     [](GLFWwindow* w)
                                     {
                                         auto& d = *static_cast<EditorWorkspace*>(glfwGetWindowUserPointer(w));
                                         d.callback([&] { d.ui.invalidate(); });
                                     });
        int frames = 0;
        while (!glfwWindowShouldClose(window))
        {
            if (demo.error)
                std::rethrow_exception(demo.error);
            demo.host.refreshMetrics();
            demo.ui.pumpTimers();
            demo.pumpPlatformServices();
            auto viewport = demo.host.metrics().viewport();
            if (viewport)
                demo.prepare(*viewport);
            if (viewport && (demo.ui.takeInvalidation() != Invalidation::None || frames == 0 || smoke))
            {
                const auto bg = demo.ui.theme().background;
                demo.beginFrameMeasurement();
                glClearColor(bg.r, bg.g, bg.b, bg.a);
                glClear(GL_COLOR_BUFFER_BIT);
                demo.canvas.beginFrame(viewport->framebufferWidth(), viewport->framebufferHeight());
                demo.ui.paint(demo.canvas, *viewport);
                demo.canvas.endFrame();
                demo.recordFrame();
                if (frames == 0 && !capture.empty())
                    screenshot(capture, viewport->framebufferWidth(), viewport->framebufferHeight());
                if (frames == 0 && smoke)
                    demo.smokeTest();
                if (glGetError() != GL_NO_ERROR)
                    throw std::runtime_error("UI host OpenGL error");
                glfwSwapBuffers(window);
                ++frames;
            }
            if (smoke)
            {
                if (frames >= 3)
                    break;
                glfwPollEvents();
            }
            else if (demo.platformPending() || (viewport && demo.ui.pendingInvalidation() != Invalidation::None))
                glfwPollEvents();
            else if (auto wakeup = demo.ui.nextWakeup())
                glfwWaitEventsTimeout(std::max(0.001, std::min(0.5, *wakeup - demo.ui.now())));
            else
                glfwWaitEvents();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
