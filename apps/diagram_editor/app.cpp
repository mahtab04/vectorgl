#include "app.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

// clang-format off
#ifdef _WIN32
#define NOMINMAX
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
// clang-format on

namespace flow
{
using vectorgl::Color;
using vectorgl::Vec2;
namespace
{
const uint32_t accentHex[] = {0x7763e8, 0x329abb, 0xd9993b, 0x409875, 0xda708c, 0xa27abb};
const char* kinds[] = {"TASK", "DECISION", "NOTE"};
Color accent(int index)
{
    return Color::hex(accentHex[index]);
}
std::filesystem::path utf8Path(const std::string& value)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}
std::string pathUtf8(const std::filesystem::path& path)
{
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
float segmentDistance(Vec2 p, Vec2 a, Vec2 b)
{
    const auto d = b - a;
    const float length = d.x * d.x + d.y * d.y;
    const float t = length > 0 ? std::clamp(((p.x - a.x) * d.x + (p.y - a.y) * d.y) / length, 0.0f, 1.0f) : 0;
    return (p - (a + d * t)).length();
}
Vec2 curve(Vec2 a, Vec2 b, float t)
{
    const float bend = std::max(70.0f, std::abs(b.x - a.x) * 0.5f), u = 1 - t;
    return a * (u * u * u) + Vec2{a.x + bend, a.y} * (3 * u * u * t) + Vec2{b.x - bend, b.y} * (3 * u * t * t) +
           b * (t * t * t);
}
#ifdef _WIN32
std::optional<std::filesystem::path> nativePath(GLFWwindow* window, bool save, bool svg)
{
    wchar_t buffer[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = glfwGetWin32Window(window);
    dialog.lpstrFilter = svg ? L"SVG image (*.svg)\0*.svg\0\0" : L"Flow Studio board (*.json)\0*.json\0\0";
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = 32768;
    dialog.lpstrDefExt = svg ? L"svg" : L"json";
    dialog.Flags =
        OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    const bool accepted = save ? GetSaveFileNameW(&dialog) != 0 : GetOpenFileNameW(&dialog) != 0;
    if (!accepted && CommDlgExtendedError())
        throw std::runtime_error("The file dialog could not be opened");
    if (!accepted)
        return std::nullopt;
    return std::filesystem::path(buffer);
}
#endif
} // namespace

App::App(GLFWwindow* window, std::string font) : window_(window), font_(std::move(font))
{
    canvas.init();
    canvas.setFontCacheLimit(20);
    if (!canvas.setFont(font_, 16))
        throw std::runtime_error("Could not load UI font. Use --font path/to/font.ttf");
    for (auto* field : {&title_, &description_, &modalField_})
    {
        field->setFont(font_, 14);
        field->style().paddingX = 12;
        field->style().paddingY = 10;
        field->style().cornerRadius = 8;
        field->style().borderWidth = 1;
        field->style().focusedBorderWidth = 1.5f;
    }
    savedDocument_ = editor_.document;
    fit();
}
Rect App::board() const
{
    return {224, 76, std::max(100.0f, width_ - 504), std::max(100.0f, height_ - 106)};
}
Vec2 App::world(Vec2 p) const
{
    return (p - pan_) * (1 / zoom_);
}
Vec2 App::screen(Vec2 p) const
{
    return p * zoom_ + pan_;
}
bool App::dirty() const
{
    return editor_.document != savedDocument_;
}
void App::fit()
{
    const auto r = board();
    float l = 0, t = 0, right = 824, bottom = 352;
    if (!editor_.document.cards.empty())
    {
        l = right = editor_.document.cards.front().x;
        t = bottom = editor_.document.cards.front().y;
        for (const auto& c : editor_.document.cards)
        {
            l = std::min(l, c.x);
            t = std::min(t, c.y);
            right = std::max(right, c.x + cardWidth);
            bottom = std::max(bottom, c.y + cardHeight);
        }
    }
    zoom_ = std::clamp(std::min((r.w - 100) / (right - l), (r.h - 150) / (bottom - t)), 0.15f, 1.2f);
    pan_ = {r.x + r.w / 2 - (l + right) * 0.5f * zoom_, r.y + r.h / 2 - (t + bottom) * 0.5f * zoom_};
}
void App::syncFields()
{
    const auto* card = selection_.size() == 1 ? editor_.document.find(selection_.front()) : nullptr;
    inspected_ = card ? card->id : 0;
    title_.setText(card ? card->title : "");
    description_.setText(card ? card->description : "");
    title_.setFocused(false);
    description_.setFocused(false);
}
void App::commitFields()
{
    if (inspected_ && (title_.focused() || description_.focused()))
    {
        const auto title = title_.text().substr(0, 1024), description = description_.text().substr(0, 4096);
        editor_.change(
            [&](Document& d)
            {
                if (auto* c = d.find(inspected_))
                {
                    c->title = title;
                    c->description = description;
                }
            });
    }
    title_.setFocused(false);
    description_.setFocused(false);
    fieldDragging_ = false;
}
void App::finishGesture()
{
    if (dragBefore_)
    {
        editor_.record(*dragBefore_);
        dragBefore_.reset();
    }
    dragging_ = panning_ = marquee_ = false;
    connecting_ = 0;
}
void App::setError(const std::string& message)
{
    error_ = message;
    modal_ = Modal::Error;
}
void App::open(const std::filesystem::path& path)
{
    auto document = readDocument(path);
    editor_.replace(std::move(document));
    path_ = path;
    savedDocument_ = editor_.document;
    selection_.clear();
    selectedEdge_ = 0;
    layerScroll_ = 0;
    syncFields();
    fit();
    status_ = "Opened " + pathUtf8(path.filename());
}
bool App::save(bool choose)
{
    if (choose || path_.empty())
    {
        fileAction(Action::SaveAs);
        return false;
    }
    const auto content = serialize(editor_.document);
    (void)parse(content); // Validate size, IDs and text before replacing a file.
    writeAtomic(path_, content);
    savedDocument_ = editor_.document;
    status_ = "Saved " + pathUtf8(path_.filename());
    return true;
}
void App::fileAction(Action kind)
{
#ifdef _WIN32
    const auto chosen = nativePath(window_, kind != Action::Open, kind == Action::Export);
    if (!chosen)
    {
        pending_ = Action::None;
        return;
    }
    if (kind == Action::Open)
        open(*chosen);
    else if (kind == Action::Export)
    {
        writeAtomic(*chosen, exportSvg(editor_.document));
        status_ = "SVG exported";
    }
    else
    {
        const auto content = serialize(editor_.document);
        (void)parse(content);
        writeAtomic(*chosen, content);
        path_ = *chosen;
        savedDocument_ = editor_.document;
        status_ = "Board saved";
        completePending();
    }
#else
    modal_ = kind == Action::Open ? Modal::PathOpen : kind == Action::Export ? Modal::PathExport : Modal::PathSave;
    modalField_.setText(kind == Action::Export ? "board.svg" : path_.empty() ? "board.json" : pathUtf8(path_));
    modalField_.setFocused(true);
    modalField_.selectAll();
#endif
}
void App::guard(Action next)
{
    if (!dirty())
    {
        action(next);
        return;
    }
    pending_ = next;
    modal_ = Modal::Unsaved;
}
void App::completePending()
{
    const auto next = pending_;
    pending_ = Action::None;
    if (next != Action::None)
        action(next);
}
void App::requestClose()
{
    glfwSetWindowShouldClose(window_, GLFW_FALSE);
    commitFields();
    finishGesture();
    guard(Action::Quit);
}
void App::action(Action kind, uint32_t value)
{
    try
    {
        switch (kind)
        {
        case Action::New:
            editor_.replace(Document{});
            path_.clear();
            savedDocument_ = editor_.document;
            selection_.clear();
            selectedEdge_ = 0;
            syncFields();
            fit();
            status_ = "New board. Add a card to begin.";
            break;
        case Action::Open:
            fileAction(kind);
            break;
        case Action::Save:
            if (save(false))
                completePending();
            break;
        case Action::SaveAs:
            fileAction(kind);
            break;
        case Action::Export:
            fileAction(kind);
            break;
        case Action::Undo:
            editor_.undo();
            selection_.clear();
            selectedEdge_ = 0;
            syncFields();
            status_ = "Undo";
            break;
        case Action::Redo:
            editor_.redo();
            selection_.clear();
            selectedEdge_ = 0;
            syncFields();
            status_ = "Redo";
            break;
        case Action::Add:
        {
            const auto r = board();
            const auto p = world({r.x + r.w / 2, r.y + r.h / 2});
            selection_ = {editor_.add(static_cast<int>(value), p.x - cardWidth / 2, p.y - cardHeight / 2)};
            selectedEdge_ = 0;
            syncFields();
            status_ = "Card added. Drag it to position.";
            break;
        }
        case Action::Delete:
            editor_.erase(selection_, selectedEdge_);
            selection_.clear();
            selectedEdge_ = 0;
            syncFields();
            break;
        case Action::Duplicate:
            selection_ = editor_.duplicate(selection_);
            syncFields();
            status_ = "Selection duplicated";
            break;
        case Action::Fit:
            fit();
            break;
        case Action::ZoomIn:
            scroll(1);
            break;
        case Action::ZoomOut:
            scroll(-1);
            break;
        case Action::Grid:
            grid_ = !grid_;
            break;
        case Action::Snap:
            snap_ = !snap_;
            break;
        case Action::Theme:
            dark_ = !dark_;
            break;
        case Action::Select:
            selection_ = {value};
            selectedEdge_ = 0;
            syncFields();
            break;
        case Action::Color:
            editor_.change(
                [&](Document& d)
                {
                    for (auto id : selection_)
                        if (auto* c = d.find(id))
                            c->color = static_cast<int>(value);
                });
            break;
        case Action::Kind:
            editor_.change(
                [&](Document& d)
                {
                    for (auto id : selection_)
                        if (auto* c = d.find(id))
                            c->kind = static_cast<int>(value);
                });
            break;
        case Action::Rename:
            modal_ = Modal::Rename;
            modalField_.setText(editor_.document.name);
            modalField_.setFocused(true);
            modalField_.selectAll();
            break;
        case Action::Help:
            modal_ = Modal::Help;
            break;
        case Action::Quit:
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
            break;
        case Action::Discard:
            modal_ = Modal::None;
            completePending();
            break;
        case Action::ModalCancel:
            modal_ = Modal::None;
            modalField_.setFocused(false);
            pending_ = Action::None;
            break;
        case Action::ModalConfirm:
        {
            const auto modal = modal_;
            modal_ = Modal::None;
            modalField_.setFocused(false);
            if (modal == Modal::Unsaved)
            {
                if (save(false))
                    completePending();
            }
            else if (modal == Modal::Rename)
                editor_.change([&](Document& d) { d.name = modalField_.text().substr(0, 256); });
            else if (modal == Modal::PathOpen)
                open(utf8Path(modalField_.text()));
            else if (modal == Modal::PathExport)
            {
                writeAtomic(utf8Path(modalField_.text()), exportSvg(editor_.document));
                status_ = "SVG exported";
            }
            else if (modal == Modal::PathSave)
            {
                const auto target = utf8Path(modalField_.text());
                const auto content = serialize(editor_.document);
                (void)parse(content);
                writeAtomic(target, content);
                path_ = target;
                savedDocument_ = editor_.document;
                status_ = "Board saved";
                completePending();
            }
            break;
        }
        default:
            break;
        }
    }
    catch (const std::exception& error)
    {
        setError(error.what());
    }
}

void App::text(const std::string& value, float x, float y, float size, Color color, float maxWidth)
{
    canvas.setFont(font_, size);
    canvas.setFillColor(color);
    if (maxWidth > 0)
    {
        canvas.save();
        canvas.clipRect(x, y, maxWidth, size * 1.5f);
        canvas.fillText(value, x, y);
        canvas.restore();
    }
    else
        canvas.fillText(value, x, y);
}
void App::box(Rect r, Color color, float radius)
{
    canvas.setFillColor(color);
    canvas.setStrokeColor(Color::Transparent);
    if (radius > 0)
        canvas.fillRoundedRect(r.x, r.y, r.w, r.h, radius);
    else
        canvas.fillRect(r.x, r.y, r.w, r.h);
}
void App::outline(Rect r, Color color, float radius, float width)
{
    canvas.setStrokeColor(color);
    canvas.setLineWidth(width);
    if (radius > 0)
        canvas.strokeRoundedRect(r.x, r.y, r.w, r.h, radius);
    else
        canvas.strokeRect(r.x, r.y, r.w, r.h);
}
void App::button(Rect r, const std::string& label, Action kind, uint32_t value, bool primary, bool enabled)
{
    const bool hover = modal_ == Modal::None && r.contains(cursor_);
    const auto foreground = primary ? Color::White : Color::hex(dark_ ? 0xe1e0ed : 0x494459);
    box(r, primary ? accent(0) : Color::hex(dark_ ? hover ? 0x353346 : 0x292737 : hover ? 0xeae7f6 : 0xf3f1f8), 8);
    auto color = foreground;
    if (!enabled)
        color.a = 0.35f;
    canvas.setFont(font_, 13);
    const float tx = r.x + std::max(8.0f, (r.w - canvas.measureText(label)) * 0.5f);
    text(label, tx, r.y + (r.h - 18) * 0.5f, 13, color, r.w - 12);
    if (enabled)
        hits_.push_back({r, kind, value});
}
void App::connection(Vec2 a, Vec2 b, Color color, float width)
{
    const float bend = std::max(70.0f, std::abs(b.x - a.x) * 0.5f);
    canvas.setStrokeColor(color);
    canvas.setLineWidth(width);
    canvas.beginPath();
    canvas.moveTo(a.x, a.y);
    canvas.bezierCurveTo(a.x + bend, a.y, b.x - bend, b.y, b.x, b.y);
    canvas.stroke();
}
void App::drawBoard()
{
    const auto r = board();
    box(r, Color::hex(dark_ ? 0x181824 : 0xf6f6fb));
    canvas.save();
    canvas.clipRect(r.x, r.y, r.w, r.h);
    if (grid_)
    {
        const float spacing = 24 * zoom_ * (zoom_ < 0.4f ? 4 : zoom_ < 0.7f ? 2 : 1);
        canvas.setFillColor(Color::hex(dark_ ? 0x353343 : 0xdeddea));
        canvas.setStrokeColor(Color::Transparent);
        for (float y = r.y + std::fmod(pan_.y - r.y, spacing); y < r.y + r.h; y += spacing)
            for (float x = r.x + std::fmod(pan_.x - r.x, spacing); x < r.x + r.w; x += spacing)
                canvas.fillCircle(x, y, 0.8f);
    }
    canvas.save();
    canvas.translate(pan_.x, pan_.y);
    canvas.scale(zoom_, zoom_);
    for (const auto& e : editor_.document.connections)
    {
        const auto* a = editor_.document.find(e.from);
        const auto* b = editor_.document.find(e.to);
        if (a && b)
            connection({a->x + cardWidth, a->y + cardHeight / 2}, {b->x, b->y + cardHeight / 2},
                       e.id == selectedEdge_ ? accent(0) : Color::hex(dark_ ? 0x706a8b : 0xb3aec9),
                       e.id == selectedEdge_ ? 3 : 2);
    }
    for (const auto& c : editor_.document.cards)
    {
        const auto position = screen({c.x, c.y});
        if (position.x + cardWidth * zoom_ < r.x || position.x > r.x + r.w || position.y + cardHeight * zoom_ < r.y ||
            position.y > r.y + r.h)
            continue;
        const bool selected = std::find(selection_.begin(), selection_.end(), c.id) != selection_.end();
        box({c.x + 1, c.y + 4, cardWidth, cardHeight}, Color{0.15f, 0.1f, 0.3f, dark_ ? 0.2f : 0.04f}, 14);
        box({c.x, c.y, cardWidth, cardHeight}, Color::hex(dark_ ? 0x272534 : c.kind == 2 ? 0xfdf7ef : 0xffffff), 14);
        outline({c.x, c.y, cardWidth, cardHeight}, selected ? accent(c.color) : Color::hex(dark_ ? 0x423e55 : 0xe6e3ef),
                14, selected ? 2 : 1);
        box({c.x + 15, c.y + 17, 24, 24}, Color{accent(c.color).r, accent(c.color).g, accent(c.color).b, 0.12f}, 7);
        text(c.kind == 1 ? "?" : c.kind == 2 ? "=" : "+", c.x + 23, c.y + 19, 16, accent(c.color));
        text(kinds[c.kind], c.x + 48, c.y + 22, 10, accent(c.color));
        text(c.title, c.x + 16, c.y + 53, 17, Color::hex(dark_ ? 0xf2efff : 0x302b43), cardWidth - 32);
        canvas.save();
        canvas.clipRect(c.x + 16, c.y + 82, cardWidth - 32, 36);
        canvas.setFont(font_, 12);
        canvas.setFillColor(Color::hex(dark_ ? 0xaaa5bd : 0x827b94));
        vectorgl::TextLayoutOptions options;
        options.maxWidth = cardWidth - 32;
        canvas.fillText(c.description, c.x + 16, c.y + 82, options);
        canvas.restore();
        for (float x : {c.x, c.x + cardWidth})
        {
            canvas.setFillColor(Color::hex(dark_ ? 0x272534 : 0xffffff));
            canvas.setStrokeColor(Color::Transparent);
            canvas.fillCircle(x, c.y + cardHeight / 2, 5);
            canvas.setStrokeColor(accent(c.color));
            canvas.setLineWidth(1.7f);
            canvas.strokeCircle(x, c.y + cardHeight / 2, 5);
        }
    }
    if (connecting_)
        if (const auto* card = editor_.document.find(connecting_))
            connection({card->x + cardWidth, card->y + cardHeight / 2}, world(cursor_), accent(card->color), 2);
    canvas.restore();
    if (marquee_)
    {
        Rect selection{std::min(start_.x, cursor_.x), std::min(start_.y, cursor_.y), std::abs(cursor_.x - start_.x),
                       std::abs(cursor_.y - start_.y)};
        auto tint = accent(0);
        tint.a = 0.08f;
        box(selection, tint);
        outline(selection, accent(0));
    }
    if (editor_.document.cards.empty())
    {
        text("A fresh space for your ideas", r.x + r.w / 2 - 140, r.y + r.h / 2 - 32, 23,
             Color::hex(dark_ ? 0xe8e3f5 : 0x4e4568));
        text("Add a task, decision or note from the library", r.x + r.w / 2 - 138, r.y + r.h / 2 + 4, 13,
             Color::hex(0x8c849f));
    }
    canvas.restore();
}

void App::themeFields(vectorgl::TextBox& field)
{
    auto& style = field.style();
    style.backgroundColor = Color::hex(dark_ ? 0x252331 : 0xfaf9fc);
    style.borderColor = Color::hex(dark_ ? 0x423d55 : 0xe6e1ee);
    style.focusedBorderColor = accent(0);
    style.caretColor = accent(0);
    style.textColor = Color::hex(dark_ ? 0xe7e2f5 : 0x493e61);
    style.placeholderColor = Color::hex(0x91879f);
}
void App::drawChrome(float dt)
{
    const auto background = Color::hex(dark_ ? 0x211f2d : 0xffffff);
    const auto foreground = Color::hex(dark_ ? 0xebe7f8 : 0x342d49);
    const auto muted = Color::hex(dark_ ? 0xa49bb8 : 0x8a819c);
    const auto border = Color::hex(dark_ ? 0x373244 : 0xece8f2);
    box({0, 0, width_, 76}, background);
    box({0, 76, 224, height_ - 106}, background);
    box({width_ - 280, 76, 280, height_ - 106}, background);
    box({0, height_ - 30, width_, 30}, background);
    box({223, 0, 1, height_ - 30}, border);
    box({width_ - 281, 76, 1, height_ - 106}, border);
    box({0, 75, width_, 1}, border);
    box({0, height_ - 31, width_, 1}, border);
    box({20, 22, 32, 32}, accent(0), 10);
    for (int i = 0; i < 4; ++i)
        box({28.0f + i % 2 * 9, 30.0f + i / 2 * 9, 6, 6}, Color::White, 2);
    text("Flow Studio", 64, 23, 19, foreground);
    text("A space for connected ideas", 20, 96, 11, muted);
    text(editor_.document.name + (dirty() ? " *" : ""), 246, 17, 18, foreground, std::max(140.0f, width_ - 790));
    text(path_.empty() ? "LOCAL WORKSPACE  /  UNSAVED FILE" : "LOCAL WORKSPACE  /  " + pathUtf8(path_.filename()), 246,
         44, 10, muted, std::max(140.0f, width_ - 790));
    hits_.push_back({{246, 12, std::max(140.0f, width_ - 810), 50}, Action::Rename});
    float bx = width_ - 486;
    button({bx, 22, 58, 32}, "New", Action::New);
    button({bx + 66, 22, 60, 32}, "Open", Action::Open);
    button({bx + 134, 22, 60, 32}, "Export", Action::Export);
    button({bx + 202, 22, 68, 32}, "Save as", Action::SaveAs);
    button({bx + 278, 22, 74, 32}, "Save", Action::Save, 0, true);
    button({bx + 366, 22, 42, 32}, dark_ ? "Light" : "Dark", Action::Theme);
    button({bx + 416, 22, 48, 32}, "Help", Action::Help);
    text("CARD LIBRARY", 20, 141, 10, muted);
    const char* labels[] = {"+  Task", "?  Decision", "=  Note"};
    const char* descriptions[] = {"A step in your workflow", "A question or branching point", "Context for your team"};
    for (int i = 0; i < 3; ++i)
    {
        const float y = 170 + i * 74.0f;
        box({16, y, 192, 62}, Color::hex(dark_ ? 0x2b273a : 0xf8f6fc), 10);
        text(labels[i], 30, y + 12, 14, accent(i == 1 ? 2 : i == 2 ? 5 : 0));
        text(descriptions[i], 30, y + 36, 11, muted);
        hits_.push_back({{16, y, 192, 62}, Action::Add, static_cast<uint32_t>(i)});
    }
    text("ON THIS BOARD", 20, 415, 10, muted);
    text(std::to_string(editor_.document.cards.size()) + " cards", 146, 415, 10, muted);
    const float available = std::max(0.0f, height_ - 550);
    layerScroll_ = std::clamp(layerScroll_, 0.0f, std::max(0.0f, editor_.document.cards.size() * 38.0f - available));
    canvas.save();
    canvas.clipRect(12, 443, 200, available);
    for (std::size_t i = 0; i < editor_.document.cards.size(); ++i)
    {
        const auto& c = editor_.document.cards[i];
        const float y = 443 + i * 38.0f - layerScroll_;
        if (y + 34 < 443 || y >= 443 + available)
            continue;
        const bool selected = std::find(selection_.begin(), selection_.end(), c.id) != selection_.end();
        if (selected)
            box({16, y, 192, 32}, Color::hex(dark_ ? 0x3a305a : 0xefebfc), 7);
        box({26, y + 12, 7, 7}, accent(c.color), 2);
        text(c.title, 45, y + 7, 12, foreground, 149);
        hits_.push_back({{16, std::max(y, 443.0f), 192, std::min(y + 32, 443 + available) - std::max(y, 443.0f)},
                         Action::Select,
                         c.id});
    }
    canvas.restore();
    button({16, height_ - 88, 90, 32}, grid_ ? "Grid on" : "Grid off", Action::Grid);
    button({114, height_ - 88, 94, 32}, snap_ ? "Snap on" : "Snap off", Action::Snap);
    const float ix = width_ - 260;
    text("PROPERTIES", ix, 103, 10, muted);
    if (!selection_.empty())
    {
        text(selection_.size() == 1 ? "Card details" : std::to_string(selection_.size()) + " cards selected", ix, 132,
             21, foreground);
        if (selection_.size() == 1)
        {
            text("TITLE", ix, 182, 10, muted);
            title_.setBounds(ix, 205, 240, 40);
            themeFields(title_);
            title_.render(canvas, dt);
            text("DESCRIPTION", ix, 266, 10, muted);
            description_.setBounds(ix, 289, 240, 40);
            themeFields(description_);
            description_.render(canvas, dt);
            text("Enter or click outside to apply", ix, 342, 11, muted);
        }
        text("CARD TYPE", ix, 388, 10, muted);
        for (int i = 0; i < 3; ++i)
            button({ix + i * 82, 411, 76, 32}, i == 0 ? "Task" : i == 1 ? "Decision" : "Note", Action::Kind, i);
        text("ACCENT COLOR", ix, 470, 10, muted);
        for (int i = 0; i < 6; ++i)
        {
            box({ix + i * 40, 497, 30, 30}, accent(i), 9);
            if (selection_.size() == 1)
                if (const auto* c = editor_.document.find(selection_.front()); c && c->color == i)
                    outline({ix + i * 40 - 3, 494, 36, 36}, accent(i), 11, 1.5f);
            hits_.push_back({{ix + i * 40, 497, 30, 30}, Action::Color, static_cast<uint32_t>(i)});
        }
        button({ix, 562, 116, 34}, "Duplicate", Action::Duplicate);
        button({ix + 124, 562, 116, 34}, "Delete", Action::Delete);
        text("CONNECTIONS", ix, 642, 10, muted);
        text("Drag from the right port of a card", ix, 669, 12, muted);
        text("to the left port of another.", ix, 688, 12, muted);
    }
    else if (selectedEdge_)
    {
        text("Connection", ix, 135, 21, foreground);
        text("Connects two cards on your board.", ix, 180, 12, muted);
        button({ix, 218, 240, 36}, "Delete connection", Action::Delete);
    }
    else
    {
        box({ix + 90, 188, 54, 54}, Color::hex(dark_ ? 0x332b49 : 0xf0ebfc), 16);
        text("+", ix + 109, 199, 27, accent(0));
        text("Make it yours", ix + 48, 265, 21, foreground);
        text("Select a card to edit its details.", ix + 24, 307, 12, muted);
        text("Shift-click to select several cards.", ix + 18, 330, 12, muted);
        text("Drag empty space to box-select.", ix + 23, 353, 12, muted);
        button({ix + 38, 397, 164, 36}, "+  Add a task", Action::Add, 0, true);
    }
    const auto r = board();
    box({r.x + 18, r.y + 16, 172, 42}, background, 10);
    button({r.x + 24, r.y + 21, 76, 32}, "Undo", Action::Undo, 0, false, editor_.canUndo());
    button({r.x + 106, r.y + 21, 76, 32}, "Redo", Action::Redo, 0, false, editor_.canRedo());
    const float zx = r.x + r.w / 2 - 120, zy = r.y + r.h - 58;
    box({zx - 6, zy - 6, 250, 46}, background, 12);
    button({zx, zy, 36, 32}, "-", Action::ZoomOut);
    text(std::to_string(static_cast<int>(zoom_ * 100)) + "%", zx + 50, zy + 7, 13, foreground);
    button({zx + 102, zy, 36, 32}, "+", Action::ZoomIn);
    button({zx + 146, zy, 92, 32}, "Fit board", Action::Fit);
    // Minimap uses the document bounds; the viewport outline follows pan/zoom.
    const Rect map{r.x + r.w - 186, r.y + r.h - 156, 166, 92};
    box({map.x - 8, map.y - 22, map.w + 16, map.h + 30}, background, 10);
    text("OVERVIEW", map.x, map.y - 15, 9, muted);
    float left = -50, top = -50, right = 874, bottom = 402;
    for (const auto& c : editor_.document.cards)
    {
        left = std::min(left, c.x - 40);
        top = std::min(top, c.y - 40);
        right = std::max(right, c.x + cardWidth + 40);
        bottom = std::max(bottom, c.y + cardHeight + 40);
    }
    const float scale = std::min(map.w / (right - left), map.h / (bottom - top));
    canvas.save();
    canvas.clipRect(map.x, map.y, map.w, map.h);
    for (const auto& c : editor_.document.cards)
        box({map.x + (c.x - left) * scale, map.y + (c.y - top) * scale, cardWidth * scale, cardHeight * scale},
            accent(c.color), 2);
    const auto view = world({r.x, r.y});
    outline({map.x + (view.x - left) * scale, map.y + (view.y - top) * scale, r.w / zoom_ * scale, r.h / zoom_ * scale},
            Color::hex(dark_ ? 0xdfd3ff : 0x4f426b), 0, 1);
    canvas.restore();
    text(status_, 18, height_ - 23, 11, muted, width_ - 420);
    text(std::to_string(editor_.document.cards.size()) + " cards   /   " +
             std::to_string(editor_.document.connections.size()) + " connections   /   " +
             (dirty()         ? "Unsaved changes"
              : path_.empty() ? "Not saved to disk"
                              : "All changes saved"),
         width_ - 396, height_ - 23, 11, muted);
}

void App::drawModal(float dt)
{
    if (modal_ == Modal::None)
        return;
    hits_.clear();
    box({0, 0, width_, height_}, Color{0.08f, 0.04f, 0.17f, 0.4f});
    const bool help = modal_ == Modal::Help;
    Rect r{width_ / 2 - 260, height_ / 2 - (help ? 210 : 140), 520, help ? 420.0f : 280.0f};
    box(r, Color::hex(dark_ ? 0x252130 : 0xffffff), 18);
    const auto foreground = Color::hex(dark_ ? 0xeee6ff : 0x392f4d), muted = Color::hex(dark_ ? 0xb0a3c2 : 0x8d7f9f);
    std::string heading = modal_ == Modal::Unsaved      ? "Save your changes?"
                          : modal_ == Modal::Rename     ? "Rename board"
                          : modal_ == Modal::Error      ? "Something needs attention"
                          : help                        ? "Find your flow"
                          : modal_ == Modal::PathOpen   ? "Open board"
                          : modal_ == Modal::PathExport ? "Export SVG"
                                                        : "Save board";
    text(heading, r.x + 28, r.y + 27, 23, foreground);
    if (help)
    {
        const char* lines[] = {"Drag a card to move it. Shift-click for multi-selection.",
                               "Drag empty space to select. Middle-drag or Space-drag to pan.",
                               "Scroll to zoom. Drag right port to left port to connect.",
                               "Ctrl+N / O / S    New / Open / Save",
                               "Ctrl+Shift+S       Save as",
                               "Ctrl+Z / Ctrl+Shift+Z    Undo / Redo",
                               "Ctrl+D / Delete   Duplicate / Delete selection",
                               "Ctrl+A               Select all cards",
                               "F                        Fit board",
                               "Double-click a card to edit its title."};
        for (int i = 0; i < 10; ++i)
            text(lines[i], r.x + 28, r.y + 76 + i * 26.0f, 12, muted);
        button({r.x + 372, r.y + r.h - 54, 120, 34}, "Got it", Action::ModalCancel, 0, true);
    }
    else if (modal_ == Modal::Unsaved)
    {
        text("Your board has changes that have not been saved.", r.x + 28, r.y + 88, 14, muted);
        text("Save before continuing, or discard these changes.", r.x + 28, r.y + 116, 13, muted);
        button({r.x + 28, r.y + 206, 108, 36}, "Discard", Action::Discard);
        button({r.x + 250, r.y + 206, 108, 36}, "Cancel", Action::ModalCancel);
        button({r.x + 370, r.y + 206, 120, 36}, "Save changes", Action::ModalConfirm, 0, true);
    }
    else if (modal_ == Modal::Error)
    {
        canvas.setFont(font_, 14);
        canvas.setFillColor(muted);
        vectorgl::TextLayoutOptions options;
        options.maxWidth = 456;
        canvas.save();
        canvas.clipRect(r.x + 28, r.y + 85, 456, 100);
        canvas.fillText(error_, r.x + 28, r.y + 85, options);
        canvas.restore();
        button({r.x + 370, r.y + 206, 120, 36}, "Close", Action::ModalCancel, 0, true);
    }
    else
    {
        text(modal_ == Modal::Rename ? "Give your workspace a clear name."
                                     : "Enter a file path. Existing files will be replaced.",
             r.x + 28, r.y + 85, 13, muted);
        modalField_.setBounds(r.x + 28, r.y + 120, 464, 44);
        themeFields(modalField_);
        modalField_.render(canvas, dt);
        button({r.x + 250, r.y + 206, 108, 36}, "Cancel", Action::ModalCancel);
        button({r.x + 370, r.y + 206, 120, 36}, "Continue", Action::ModalConfirm, 0, true);
    }
}
void App::render(float width, float height, float dpi, float dt)
{
    width_ = width;
    height_ = height;
    hits_.clear();
    canvas.beginFrame(static_cast<int>(std::round(width * dpi)), static_cast<int>(std::round(height * dpi)));
    canvas.scale(dpi, dpi);
    drawBoard();
    drawChrome(dt);
    drawModal(dt);
    canvas.endFrame();
    const auto title = "Flow Studio - " + editor_.document.name + (dirty() ? " *" : "");
    glfwSetWindowTitle(window_, title.c_str());
}

void App::pointer(float x, float y)
{
    cursor_ = {x, y};
    if (fieldDragging_)
    {
        if (modalField_.focused())
            modalField_.handlePointerDrag(x, y, canvas);
        else if (title_.focused())
            title_.handlePointerDrag(x, y, canvas);
        else if (description_.focused())
            description_.handlePointerDrag(x, y, canvas);
    }
    if (panning_)
        pan_ = pan_ + cursor_ - last_;
    else if (dragging_ && dragBefore_)
    {
        const auto delta = world(cursor_) - dragWorld_;
        for (auto id : selection_)
            if (auto* c = editor_.document.find(id))
                if (const auto* original = dragBefore_->find(id))
                {
                    c->x = std::clamp(original->x + delta.x, -999000.0f, 999000.0f);
                    c->y = std::clamp(original->y + delta.y, -999000.0f, 999000.0f);
                    if (snap_)
                    {
                        c->x = std::round(c->x / 12) * 12;
                        c->y = std::round(c->y / 12) * 12;
                    }
                }
    }
    last_ = cursor_;
}
void App::mouse(int button, int state, int modifiers)
{
    if (state == GLFW_RELEASE)
    {
        fieldDragging_ = false;
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            if (connecting_ && board().contains(cursor_))
            {
                const auto point = world(cursor_);
                for (const auto& c : editor_.document.cards)
                    if ((point - Vec2{c.x, c.y + cardHeight / 2}).length() < 14 / zoom_)
                    {
                        action(Action::None);
                        try
                        {
                            editor_.connect(connecting_, c.id);
                            status_ = "Connection created";
                        }
                        catch (const std::exception& error)
                        {
                            setError(error.what());
                        }
                        break;
                    }
            }
            if (marquee_)
            {
                const auto a = world(start_), b = world(cursor_);
                const Rect selected{std::min(a.x, b.x), std::min(a.y, b.y), std::abs(a.x - b.x), std::abs(a.y - b.y)};
                for (const auto& c : editor_.document.cards)
                    if (c.x <= selected.x + selected.w && c.x + cardWidth >= selected.x &&
                        c.y <= selected.y + selected.h && c.y + cardHeight >= selected.y)
                        if (std::find(selection_.begin(), selection_.end(), c.id) == selection_.end())
                            selection_.push_back(c.id);
                syncFields();
            }
            finishGesture();
        }
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
            panning_ = false;
        return;
    }
    if (state != GLFW_PRESS)
        return;
    if (modal_ != Modal::None)
    {
        if (button != GLFW_MOUSE_BUTTON_LEFT)
            return;
        for (const auto& hit : hits_)
            if (hit.rect.contains(cursor_))
            {
                action(hit.action, hit.value);
                return;
            }
        if (modalField_.focused())
        {
            modalField_.handlePointerDown(cursor_.x, cursor_.y, canvas);
            fieldDragging_ = modalField_.focused();
        }
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && selection_.size() == 1)
    {
        if (title_.hitTest(cursor_.x, cursor_.y))
        {
            commitFields();
            title_.handlePointerDown(cursor_.x, cursor_.y, canvas);
            fieldDragging_ = true;
            return;
        }
        if (description_.hitTest(cursor_.x, cursor_.y))
        {
            commitFields();
            description_.handlePointerDown(cursor_.x, cursor_.y, canvas);
            fieldDragging_ = true;
            return;
        }
    }
    commitFields();
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        for (const auto& hit : hits_)
            if (hit.rect.contains(cursor_))
            {
                if (hit.action == Action::New || hit.action == Action::Open)
                    guard(hit.action);
                else
                    action(hit.action, hit.value);
                return;
            }
    if (!board().contains(cursor_))
        return;
    if (button == GLFW_MOUSE_BUTTON_MIDDLE ||
        (button == GLFW_MOUSE_BUTTON_LEFT && glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS))
    {
        panning_ = true;
        last_ = cursor_;
        return;
    }
    if (button != GLFW_MOUSE_BUTTON_LEFT)
        return;
    const auto point = world(cursor_);
    for (auto it = editor_.document.cards.rbegin(); it != editor_.document.cards.rend(); ++it)
    {
        if ((point - Vec2{it->x + cardWidth, it->y + cardHeight / 2}).length() < 12 / zoom_)
        {
            connecting_ = it->id;
            selectedEdge_ = 0;
            return;
        }
        if (Rect{it->x, it->y, cardWidth, cardHeight}.contains(point))
        {
            const auto selected = std::find(selection_.begin(), selection_.end(), it->id);
            if (modifiers & GLFW_MOD_SHIFT)
            {
                if (selected == selection_.end())
                    selection_.push_back(it->id);
                else
                    selection_.erase(selected);
            }
            else if (selected == selection_.end())
                selection_ = {it->id};
            selectedEdge_ = 0;
            syncFields();
            if (std::find(selection_.begin(), selection_.end(), it->id) != selection_.end())
            {
                dragging_ = true;
                dragBefore_ = editor_.document;
                dragWorld_ = point;
            }
            static double previousClick = 0;
            static uint32_t previousCard = 0;
            const double now = glfwGetTime();
            if (previousCard == it->id && now - previousClick < 0.35)
            {
                title_.setFocused(true);
                title_.selectAll();
                dragging_ = false;
                dragBefore_.reset();
            }
            previousClick = now;
            previousCard = it->id;
            return;
        }
    }
    for (auto it = editor_.document.connections.rbegin(); it != editor_.document.connections.rend(); ++it)
    {
        const auto* a = editor_.document.find(it->from);
        const auto* b = editor_.document.find(it->to);
        if (!a || !b)
            continue;
        const Vec2 from{a->x + cardWidth, a->y + cardHeight / 2}, to{b->x, b->y + cardHeight / 2};
        Vec2 previous = from;
        for (int i = 1; i <= 32; ++i)
        {
            const auto next = curve(from, to, i / 32.0f);
            if (segmentDistance(point, previous, next) < 6 / zoom_)
            {
                selectedEdge_ = it->id;
                selection_.clear();
                syncFields();
                return;
            }
            previous = next;
        }
    }
    if (!(modifiers & GLFW_MOD_SHIFT))
        selection_.clear();
    selectedEdge_ = 0;
    syncFields();
    marquee_ = true;
    start_ = cursor_;
}
void App::scroll(float amount)
{
    if (modal_ != Modal::None || dragging_ || connecting_)
        return;
    if (cursor_.x < 224 && cursor_.y > 443)
    {
        layerScroll_ -= amount * 38;
        return;
    }
    Vec2 anchor = cursor_;
    const auto r = board();
    if (!r.contains(anchor))
        anchor = {r.x + r.w / 2, r.y + r.h / 2};
    const auto before = world(anchor);
    zoom_ = std::clamp(zoom_ * std::pow(1.12f, amount), 0.15f, 3.0f);
    pan_ = anchor - before * zoom_;
}
void App::character(uint32_t cp)
{
    auto* field = modal_ != Modal::None ? &modalField_ : title_.focused() ? &title_ : &description_;
    const std::size_t limit = field == &description_    ? 4096
                              : field == &title_        ? 1024
                              : modal_ == Modal::Rename ? 256
                                                        : 4096;
    if (field->focused() && field->text().size() + 4 <= limit)
        field->handleCharInput(cp);
}
void App::key(int code, int state, int modifiers)
{
    if (state != GLFW_PRESS && state != GLFW_REPEAT)
        return;
    const bool control = (modifiers & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER)) != 0;
    const bool shift = (modifiers & GLFW_MOD_SHIFT) != 0;
    auto* field = modal_ != Modal::None && modalField_.focused() ? &modalField_
                  : title_.focused()                             ? &title_
                  : description_.focused()                       ? &description_
                                                                 : nullptr;
    if (field)
    {
        if (control && code == GLFW_KEY_A)
        {
            field->selectAll();
            return;
        }
        if (control && (code == GLFW_KEY_C || code == GLFW_KEY_X))
        {
            std::string value;
            if (code == GLFW_KEY_C ? field->copySelection(value) : field->cutSelection(value))
                glfwSetClipboardString(window_, value.c_str());
            return;
        }
        if (control && code == GLFW_KEY_V)
        {
            const char* value = glfwGetClipboardString(window_);
            const std::size_t limit = field == &description_    ? 4096
                                      : field == &title_        ? 1024
                                      : modal_ == Modal::Rename ? 256
                                                                : 4096;
            if (value && field->text().size() + std::char_traits<char>::length(value) <= limit)
                field->pasteText(value);
            return;
        }
        using Key = vectorgl::TextBoxKey;
        if (code == GLFW_KEY_ENTER)
        {
            if (modal_ != Modal::None)
                action(Action::ModalConfirm);
            else
                commitFields();
            return;
        }
        if (code == GLFW_KEY_ESCAPE)
        {
            if (modal_ != Modal::None)
                action(Action::ModalCancel);
            else
                syncFields();
            return;
        }
        if (code == GLFW_KEY_TAB && modal_ == Modal::None)
        {
            commitFields();
            auto& next = field == &title_ ? description_ : title_;
            next.setFocused(true);
            next.selectAll();
            return;
        }
        const std::pair<int, Key> mapped[] = {{GLFW_KEY_BACKSPACE, Key::Backspace}, {GLFW_KEY_DELETE, Key::Delete},
                                              {GLFW_KEY_LEFT, Key::Left},           {GLFW_KEY_RIGHT, Key::Right},
                                              {GLFW_KEY_HOME, Key::Home},           {GLFW_KEY_END, Key::End}};
        for (const auto& [key, action] : mapped)
            if (code == key)
            {
                field->handleKey(action, shift);
                return;
            }
        if (!(control && (code == GLFW_KEY_S || code == GLFW_KEY_Z)))
            return;
        commitFields();
    }
    if (modal_ != Modal::None)
    {
        if (code == GLFW_KEY_ESCAPE)
            action(Action::ModalCancel);
        return;
    }
    finishGesture();
    if (control)
    {
        switch (code)
        {
        case GLFW_KEY_N:
            guard(Action::New);
            break;
        case GLFW_KEY_O:
            guard(Action::Open);
            break;
        case GLFW_KEY_S:
            action(shift ? Action::SaveAs : Action::Save);
            break;
        case GLFW_KEY_Z:
            action(shift ? Action::Redo : Action::Undo);
            break;
        case GLFW_KEY_Y:
            action(Action::Redo);
            break;
        case GLFW_KEY_D:
            action(Action::Duplicate);
            break;
        case GLFW_KEY_A:
            selection_.clear();
            for (const auto& c : editor_.document.cards)
                selection_.push_back(c.id);
            selectedEdge_ = 0;
            syncFields();
            break;
        default:
            break;
        }
    }
    else if (code == GLFW_KEY_DELETE || code == GLFW_KEY_BACKSPACE)
        action(Action::Delete);
    else if (code == GLFW_KEY_F)
        action(Action::Fit);
    else if (code == GLFW_KEY_ESCAPE)
    {
        selection_.clear();
        selectedEdge_ = 0;
        syncFields();
    }
}

void App::smokeTest(const std::filesystem::path& directory)
{
    const auto original = editor_.document;
    // Exercise pointer dragging and transaction boundaries on the rendered UI.
    const auto point = screen({original.cards.front().x + 100, original.cards.front().y + 40});
    pointer(point.x, point.y);
    mouse(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    pointer(point.x + 36, point.y + 24);
    mouse(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
    if (editor_.document == original)
        throw std::runtime_error("Smoke pointer drag did not move the card");
    action(Action::Undo);
    if (editor_.document != original)
        throw std::runtime_error("Smoke pointer drag did not create one undo transaction");
    const auto added = editor_.add(2, -60, -120);
    if (!editor_.connect(added, original.cards.front().id))
        throw std::runtime_error("Smoke connection failed");
    selection_ = {added};
    syncFields();
    title_.setFocused(true);
    title_.selectAll();
    title_.pasteText("Saved Unicode: Caf\xc3\xa9");
    commitFields();
    selection_ = editor_.duplicate(selection_);
    const auto duplicated = editor_.document;
    action(Action::Delete);
    action(Action::Undo);
    if (editor_.document != duplicated)
        throw std::runtime_error("Smoke undo failed");
    action(Action::Redo);
    path_ = directory / "flow-smoke.json";
    if (!save(false))
        throw std::runtime_error("Smoke save failed");
    const auto saved = editor_.document;
    open(path_);
    if (editor_.document != saved || dirty())
        throw std::runtime_error("Smoke reload failed");
    bool rejected = false;
    writeAtomic(directory / "invalid.json", "{}");
    try
    {
        open(directory / "invalid.json");
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    if (!rejected || editor_.document != saved)
        throw std::runtime_error("Invalid open replaced the current document");
    writeAtomic(directory / "flow-smoke.svg", exportSvg(editor_.document));
    editor_.add(0, 0, 0);
    requestClose();
    if (modal_ != Modal::Unsaved || glfwWindowShouldClose(window_))
        throw std::runtime_error("Dirty board close did not request save confirmation");
    action(Action::ModalCancel);
    editor_.replace(original);
    selection_.clear();
    syncFields();
    fit();
    savedDocument_ = original;
    path_.clear();
}
} // namespace flow
