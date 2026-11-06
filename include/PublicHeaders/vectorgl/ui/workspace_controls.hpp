#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "vectorgl/text.hpp"
#include "vectorgl/ui/controls.hpp"
namespace vectorgl::ui
{
enum class TextOverflow
{
    Clip,
    Ellipsis,
    Wrap
};
/*! Codepoint-safe single-line ellipsis. Measurement must return finite nonnegative
 * widths with monotonic prefix widths. No font/window is needed by this helper. */
std::string ellipsizeText(std::string_view text, float width, const std::function<float(std::string_view)>& measure);
struct LabelOptions
{
    std::string text;
    TextOverflow overflow = TextOverflow::Ellipsis;
    TextAlign align = TextAlign::Left;
    Padding padding;
    float fontScale = 1;
    Size fallbackSize{160, 24};
};
/*! Context/Canvas must outlive Label. With a configured Theme font, measurement
 * and painting need an initialized Canvas/current GL context. Otherwise measure
 * uses fallbackSize. Owns a registered, pointer-transparent label. */
class Label
{
public:
    Label(Context& context, Canvas& canvas, WidgetId parent = {}, LabelOptions options = {});
    ~Label();
    Label(const Label&) = delete;
    Label& operator=(const Label&) = delete;
    WidgetId id() const;
    const std::string& text() const;
    void setText(std::string text);
    void setOverflow(TextOverflow overflow);

private:
    struct State;
    std::shared_ptr<State> state_;
};
enum class SeparatorAxis
{
    Horizontal,
    Vertical
};
struct SeparatorOptions
{
    SeparatorAxis axis = SeparatorAxis::Horizontal;
    float thickness = 1, length = 24, inset = 0;
};
class Separator
{
public:
    Separator(Context& context, WidgetId parent = {}, SeparatorOptions options = {});
    ~Separator();
    Separator(const Separator&) = delete;
    Separator& operator=(const Separator&) = delete;
    WidgetId id() const;

private:
    Context& ui_;
    WidgetId id_;
};
struct TabItem
{
    std::string label;
    bool enabled = true;
};
struct TabBarOptions
{
    std::vector<TabItem> tabs;
    std::optional<std::size_t> selected; // Constructor defaults to first enabled tab.
    Size tabSize{120, 40};
    std::function<void(std::size_t)> onChanged;
};
/*! Owns a bar/buttons with a roving Tab stop. Arrows/Home/End select enabled tabs.
 * Programmatic setters are silent. Context must outlive the controller. */
class TabBar
{
public:
    TabBar(Context& context, WidgetId parent = {}, TabBarOptions options = {});
    ~TabBar();
    TabBar(const TabBar&) = delete;
    TabBar& operator=(const TabBar&) = delete;
    WidgetId id() const;
    WidgetId tabId(std::size_t index) const;
    std::optional<std::size_t> selected() const;
    void setSelected(std::optional<std::size_t> index); // nullopt clears selection.
    void setTabEnabled(std::size_t index, bool enabled);

private:
    struct State;
    std::shared_ptr<State> state_;
};
/*! Composes a TabBar and owned page containers. Add layouts/children to page(i).
 * Only the selected page participates in layout, input and painting. */
class TabView
{
public:
    TabView(Context& context, WidgetId parent = {}, TabBarOptions options = {});
    ~TabView();
    TabView(const TabView&) = delete;
    TabView& operator=(const TabView&) = delete;
    WidgetId id() const;
    WidgetId barId() const;
    WidgetId tabId(std::size_t index) const;
    WidgetId page(std::size_t index) const;
    std::optional<std::size_t> selected() const;
    void setSelected(std::optional<std::size_t> index);
    void setTabEnabled(std::size_t index, bool enabled);

private:
    struct State;
    std::shared_ptr<State> state_;
};
struct SectionOptions
{
    std::string title;
    bool expanded = true;
    Size headerSize{240, 44};
    std::function<void(bool)> onChanged;
};
class CollapsibleSection
{
public:
    CollapsibleSection(Context& context, WidgetId parent = {}, SectionOptions options = {});
    ~CollapsibleSection();
    CollapsibleSection(const CollapsibleSection&) = delete;
    CollapsibleSection& operator=(const CollapsibleSection&) = delete;
    WidgetId id() const;
    WidgetId headerId() const;
    WidgetId content() const;
    bool expanded() const;
    void setExpanded(bool expanded); // Silent; collapse returns descendant focus to header.
private:
    struct State;
    std::shared_ptr<State> state_;
};
struct SearchFieldOptions
{
    std::string query, placeholder = "Search...";
    Size preferredSize{240, 44};
    std::function<void(std::string_view)> onChanged;
    std::function<void(const ServiceResult&)> onServiceError;
};
/*! Live query + clear button. Reuses TextField's Unicode, clipboard and
 * Escape-restore contracts. Context/Canvas must outlive the controller. */
class SearchField
{
public:
    SearchField(Context& context, Canvas& canvas, WidgetId parent = {}, SearchFieldOptions options = {});
    ~SearchField();
    SearchField(const SearchField&) = delete;
    SearchField& operator=(const SearchField&) = delete;
    WidgetId id() const;
    WidgetId fieldId() const;
    WidgetId clearId() const;
    const std::string& query() const;
    void setQuery(std::string query); // Silent replacement of buffer/committed query.
    void clear();                     // Emits once if nonempty, returning focus to the editor.
private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace vectorgl::ui
