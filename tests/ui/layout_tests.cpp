#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vectorgl/ui/context.hpp>

using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
void near(float actual, float expected)
{
    require(std::abs(actual - expected) < 0.01f, "layout dimension mismatch");
}
template <class Exception, class Function> void rejects(Function f)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const Exception&)
    {
        caught = true;
    }
    require(caught, "expected layout exception");
}
LayoutItem item(float width, float height)
{
    return {{}, [=](Constraints) { return Size{width, height}; }};
}
LayoutItem wrapping()
{
    return {{},
            [](Constraints c)
            {
                const float w = std::isfinite(c.maximum.width) ? std::max(1.0f, c.maximum.width) : 200;
                return Size{std::min(200.0f, w), std::ceil(200 / w) * 10};
            }};
}
void linearLayouts()
{
    RowLayout row({10, 10, 10, 10}, 10);
    std::vector<LayoutItem> items{item(40, 20), item(40, 30)};
    items[0].hints.flex = 1;
    items[0].hints.maximum.width = 60;
    items[1].hints.flex = 2;
    auto boxes = row.arrange(items, {300, 100});
    near(boxes[0].x, 10);
    near(boxes[0].width, 60);
    near(boxes[1].x, 80);
    near(boxes[1].width, 210);
    near(boxes[0].height, 80);
    items = {item(100, 20), item(100, 20)};
    for (auto& value : items)
        value.hints.minimum.width = 40;
    boxes = RowLayout({}, 10).arrange(items, {100, 40});
    near(boxes[0].width, 45);
    near(boxes[1].width, 45);
    items[0].hints.width = 100;
    items[1].hints.width = 80;
    boxes = RowLayout({}, 10).arrange(items, {100, 40});
    near(boxes[0].width, 100);
    near(boxes[1].width, 80);
    near(boxes[1].x, 110);
    items = {wrapping(), wrapping()};
    ColumnLayout column({10, 10, 10, 10}, 4);
    near(column.measure(items, {{}, {120, std::numeric_limits<float>::infinity()}}).height, 64);
    near(column.measure(items, {{}, {70, std::numeric_limits<float>::infinity()}}).height, 104);
    near(RowLayout().measure(items, {{}, {100, 1000}}).height, 40);
    items = {item(30, 20)};
    items[0].hints.align = Alignment::End;
    boxes = ColumnLayout().arrange(items, {100, 100});
    near(boxes[0].x, 70);
    near(boxes[0].width, 30);
    near(ColumnLayout({2, 3, 4, 5}, 8).measure({}, {}).width, 6);
    near(ColumnLayout({2, 3, 4, 5}, 8).measure({}, {}).height, 8);
}
void grids()
{
    GridLayout grid({GridTrack::fixed(50), GridTrack::automatic(), GridTrack::fraction()},
                    {GridTrack::automatic(), GridTrack::fixed(30)}, {10, 10, 10, 10}, 5, 4);
    std::vector<LayoutItem> items{item(40, 20), item(100, 20), item(100, 20)};
    items[0].hints.cell = {0, 1};
    items[1].hints.cell = {0, 2};
    items[2].hints.cell = {1, 2};
    auto boxes = grid.arrange(items, {300, 100});
    near(boxes[0].x, 65);
    near(boxes[0].width, 40);
    near(boxes[1].x, 110);
    near(boxes[1].width, 180);
    near(boxes[2].y, 34);
    near(boxes[2].height, 30);
    GridLayout caps({GridTrack::fraction(1, 0, 60), GridTrack::fraction()}, {GridTrack::fixed(20)});
    items = {item(0, 0), item(0, 0)};
    items[1].hints.cell.column = 1;
    boxes = caps.arrange(items, {200, 20});
    near(boxes[0].width, 60);
    near(boxes[1].width, 140);
    GridLayout spans({GridTrack::fraction(), GridTrack::fraction()}, {GridTrack::automatic()});
    items = {wrapping()};
    items[0].hints.cell.columnSpan = 2;
    items[0].hints.minimum.width = 120;
    boxes = spans.arrange(items, {100, 100});
    near(boxes[0].width, 120); // Minimum overflows.
    near(spans.measure(items, {{}, {200, std::numeric_limits<float>::infinity()}}).width, 200);
    near(spans.measure(items, {{}, {200, std::numeric_limits<float>::infinity()}}).height, 10);
    GridLayout empty({GridTrack::fixed(30), GridTrack::fraction()}, {GridTrack::fixed(20)});
    near(empty.measure({}, {{}, {100, 100}}).width, 100);
    near(empty.measure({}, {}).width, 30);
    GridLayout rowSpan({GridTrack::fixed(60)}, {GridTrack::fixed(20), GridTrack::automatic()});
    items = {item(30, 70)};
    items[0].hints.cell.rowSpan = 2;
    boxes = rowSpan.arrange(items, {60, 100});
    near(boxes[0].height, 70);
    items[0].hints.cell.columnSpan = 2;
    rejects<std::invalid_argument>([&] { rowSpan.arrange(items, {60, 100}); });
}
void integration()
{
    Context ui;
    Widget root;
    root.layout = std::make_shared<RowLayout>(Padding{10, 10, 10, 10}, 10);
    root.clipChildren = true;
    root.transform = Mat3x3::translation(100, 50);
    auto rootId = ui.add(root);
    int measured = 0;
    Widget child;
    child.parent = rootId;
    child.focusable = true;
    child.layoutHints.flex = 1;
    child.transform = Mat3x3::rotation(0.1f);
    child.onMeasure = [&](const MeasureContext& c)
    {
        ++measured;
        return Size{40, c.theme.fontSize};
    };
    const auto first = ui.add(child);
    const auto second = ui.add(child);
    ui.layout(rootId, {20, 30, 200, 100});
    near(ui.layoutBounds(first)->x, 10);
    near(ui.bounds(first)->x, 0);
    near(ui.bounds(first)->width, 85);
    const auto point = (Mat3x3::translation(20, 30) * root.transform * Mat3x3::translation(10, 10) * child.transform)
                           .transformPoint({10, 10});
    require(ui.hitTest(point) == first, "layout placement did not participate in hit transform");
    require(ui.layoutStats().arrangedWidgets == 3, "arrangement stats incorrect");
    const auto count = measured;
    ui.takeInvalidation();
    ui.layout(rootId, {20, 30, 200, 100});
    require(measured == count && ui.layoutStats().arrangedWidgets == 0 &&
                ui.pendingInvalidation() == Invalidation::None,
            "unchanged layout remeasured or invalidated paint");
    ui.setEnabled(first, false);
    ui.layout(rootId, {20, 30, 200, 100});
    near(ui.layoutBounds(second)->x, 105);
    require(measured == count, "disabled widget changed layout");
    ui.setVisible(first, false);
    ui.layout(rootId, {20, 30, 200, 100});
    near(ui.layoutBounds(second)->x, 10);
    near(ui.bounds(second)->width, 180);
    auto theme = ui.theme();
    theme.fontSize += 5;
    ui.setTheme(theme);
    ui.takeInvalidation();
    ui.layout(rootId, {20, 30, 200, 100});
    require(measured > count, "theme invalidation lost after dirty flags consumed");
    near(ui.measure(second).height, theme.fontSize);
    ui.invalidate(Invalidation::Measure);
    const auto before = measured;
    ui.layout(rootId, {20, 30, 200, 100});
    require(measured > before, "explicit content invalidation did not remeasure");
    ui.remove(first);
    ui.layout(rootId, {20, 30, 100, 100});
    near(ui.bounds(second)->width, 80);
}
void failureAndCompatibility()
{
    Context ui;
    Widget root;
    root.layout = std::make_shared<ColumnLayout>();
    auto id = ui.add(root);
    Widget child;
    child.parent = id;
    child.bounds = {7, 9, 80, 20};
    auto leaf = ui.add(child);
    ui.layout(id, {0, 0, 100, 100});
    const auto previous = ui.layoutBounds(leaf);
    ui.setMeasureCallback(leaf,
                          [&](const MeasureContext&)
                          {
                              ui.remove(leaf);
                              return Size{};
                          });
    rejects<std::logic_error>([&] { ui.layout(id, {0, 0, 200, 100}); });
    require(ui.contains(leaf) && ui.layoutBounds(leaf) == previous, "failed measurement published partial geometry");
    ui.setMeasureCallback(leaf,
                          [](const MeasureContext&) { return Size{80, std::numeric_limits<float>::quiet_NaN()}; });
    rejects<std::invalid_argument>([&] { ui.layout(id, {0, 0, 200, 100}); });
    ui.setMeasureCallback(leaf, {});
    ui.layout(id, {0, 0, 200, 100});
    near(ui.bounds(leaf)->height, 20);
    struct BrokenLayout final : Layout
    {
        Size measure(std::span<const LayoutItem>, Constraints) const override
        {
            return {};
        }
        std::vector<Bounds> arrange(std::span<const LayoutItem>, Size) const override
        {
            return {};
        }
    };
    const auto rootBefore = ui.layoutBounds(id), leafBefore = ui.layoutBounds(leaf);
    ui.setLayout(id, std::make_shared<BrokenLayout>());
    rejects<std::invalid_argument>([&] { ui.layout(id, {0, 0, 300, 100}); });
    require(ui.layoutBounds(id) == rootBefore && ui.layoutBounds(leaf) == leafBefore,
            "arrangement failure changed published geometry");
    ui.setLayout(id, std::make_shared<ColumnLayout>());
    ui.layout(id, {0, 0, 200, 100});
    // Placement never becomes the fallback intrinsic size after a stretch.
    near(ui.measure(leaf).width, 80);
    ui.setBounds(leaf, {7, 9, 33, 44});
    require(ui.layoutBounds(leaf) == Bounds{7, 9, 33, 44}, "manual bounds did not reset placement");
    Context legacy;
    Widget panel;
    panel.bounds = {100, 100, 50, 50};
    auto panelId = legacy.add(panel);
    Widget manual;
    manual.parent = panelId;
    manual.bounds = {10, 10, 20, 20};
    auto manualId = legacy.add(manual);
    require(legacy.hitTest({15, 15}) == manualId, "unmanaged legacy bounds changed coordinate meaning");
    LayoutHints bad;
    bad.minimum.width = 10;
    bad.maximum.width = 5;
    rejects<std::invalid_argument>([&] { ui.setLayoutHints(leaf, bad); });
    rejects<std::invalid_argument>([] { GridLayout invalid({}, {GridTrack::automatic()}); });
    rejects<std::invalid_argument>([] { RowLayout invalid({}, -1); });
    rejects<std::invalid_argument>([] { GridLayout invalid({GridTrack::fraction(0)}, {GridTrack::automatic()}); });
}
} // namespace
int main()
{
    try
    {
        linearLayouts();
        grids();
        integration();
        failureAndCompatibility();
        std::cout << "Layout behavior tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
