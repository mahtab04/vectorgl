# Automatic UI layout

`vectorgl::ui` provides a headless measure/arrange engine with immutable `RowLayout`,
`ColumnLayout`, and `GridLayout` strategies. No new dependency or CMake option is
required beyond `VECTORGL_BUILD_UI=ON`. Existing manually positioned widgets keep
their coordinate behavior until explicitly arranged.

## Layout ownership and coordinates

Attach a layout strategy to a container and use layout hints on its direct children:

```cpp
#include <vectorgl/ui/context.hpp>

using namespace vectorgl::ui;
Context ui;
Widget panel;
panel.layout = std::make_shared<RowLayout>(Padding{12, 12, 12, 12}, 8);
panel.clipChildren = true;
auto root = ui.add(panel);

Widget label;
label.parent = root;
label.onMeasure = [](const MeasureContext&) { return Size{80, 24}; };
auto labelId = ui.add(label);

Widget field;
field.parent = root;
field.layoutHints.minimum = {120, 40};
field.layoutHints.flex = 1;
auto fieldId = ui.add(field);

ui.layout(root, {20, 30, 400, 72}); // Parent logical coordinates.
```

`layout()` assigns the root and each managed child a local `{0,0,width,height}`
geometry plus an internal placement offset. The world matrix composes:

```text
parentWorld * translation(layoutOffset) * applicationTransform
```

`Widget::transform` and `setTransform()` remain application-owned. Bounds x/y are
geometry rather than a child-origin translation, as in the earlier UI API.
`bounds(id)` returns local geometry; `layoutBounds(id)` includes placement before
the application transform. `mapFromRoot()`, pointer local coordinates, painting,
ancestor clipping, and hit testing all account for placement.

Only visible direct children participate. Hidden children retain their last bounds
and leave the flow until shown and rearranged. Disabled children retain space.
Containers without a strategy keep manual child placement; nested containers with
their own strategies still arrange within their current bounds.

Calling `setBounds()` resets that widget's layout offset and updates its fallback
intrinsic size. Its parent strategy will manage it again on the next layout call.
Clearing a container's strategy retains its children's last arranged geometry;
use explicit bounds to resume manual positioning.

## Measurement and constraints

`Constraints` has finite nonnegative minima and maxima that may be positive infinity.
`Context::measure(id, constraints)` reports a desired constrained size without
publishing geometry. Measurement uses:

- A container's layout strategy for visible child content.
- `Widget::onMeasure` for intrinsic application content, receiving constraints and
  the current Theme. If both are present, the larger desired size on each axis wins.
- The last manually supplied width/height as fallback. An allocated stretch size
  never becomes the widget's intrinsic size.

Callbacks must return finite, nonnegative sizes. They should depend on content,
theme, and offered constraints, with repeatable results for the same inputs. They
must not paint, mutate Context, dispatch events, or reenter measure/arrange. A
context mutation attempt throws `logic_error`. They may use an application-owned
text metrics provider; the engine itself loads no fonts or GL resources. The
graphical example uses Canvas text metrics with a current GL context for font loading.
Headless tests use deterministic measurement providers.

Width-dependent content should measure wrapping against `constraints.maximum.width`.
An unbounded width requests natural width. The strategies remeasure after width
allocation so wrapping can determine row/column height. Custom content must call
`invalidate(Invalidation::Measure)` when its intrinsic content changes.

`LayoutHints` specifies minimum/maximum sizes, optional fixed `width`/`height`,
nonnegative main-axis `flex`, optional alignment, and grid placement. Fixed sizes
are clamped to the widget's minimum/maximum. Widget size limits take precedence
over offers that cannot accommodate them.

## Rows and columns

Rows and columns support padding, gaps, and Start/Center/End/Stretch cross-axis
alignment. Alignment defaults to Stretch and can be overridden per child.

The main-axis basis comes from constrained intrinsic sizes. Positive remaining
space is divided by flex weight, redistributing unused shares when a maximum is
reached. Without flex, surplus space remains after the children. A deficit shrinks
children proportionally to their available size above their minima. Fixed dimensions
do not shrink. Minima remain intact when the container cannot accommodate them;
remaining overflow starts at the padded origin. Clipping is an explicit container
choice. Scrolling and split panels are a later feature.

Column widths are resolved before height measurement. Rows remeasure children
at their allocated widths. Transforms affect painting/hits and do not inflate a
widget's logical layout size. Containers use one line/column; responsive strategy
switching is an application decision, demonstrated in the example.

## Grids

```cpp
panel.layout = std::make_shared<GridLayout>(
    std::vector<GridTrack>{GridTrack::automatic(), GridTrack::fraction()},
    std::vector<GridTrack>{GridTrack::automatic(), GridTrack::automatic()},
    Padding{12,12,12,12}, 10, 8);

field.layoutHints.cell = {0, 1};       // Row, column.
label.layoutHints.cell = {1, 0, 1, 2}; // Row/column spans.
```

Grid tracks are explicit: Fixed, Auto, and weighted Fraction, each with optional
minimum/maximum. Fixed tracks retain their clamped value. Auto tracks take intrinsic
requirements; fraction tracks take remaining space after gaps, padding, and track
bases, respecting maxima. Under an unbounded offer, fractions use intrinsic content.
Empty explicit tracks still contribute their size.

Spanning intrinsic requirements grow Auto tracks first. Without Auto tracks,
unbounded fractions can grow for intrinsic content. Minimum requirements distribute
across nonfixed tracks. Requirements are processed in item order. Width allocation
precedes row measurement so wrapped content sets Auto row height. Explicit track
counts are limited to 1..4096 per axis. Out-of-range cells and zero spans are rejected.
Overlapping cells use normal widget paint order. Implicit placement, baseline
alignment, and a CSS sizing engine are outside this slice.

## Invalidation, caching, and failures

Call `layout()` before painting and hit testing when layout-affecting data changes.
The graphical host calls it after resize/content input and before each paint.
Consuming `takeInvalidation()` does not lose pending measurement changes: layout
tracks its input revision separately from repaint flags.

Child registration/removal/visibility, intrinsic bounds, hints, strategy, measurement
callback, theme, and explicit Measure/Arrange invalidation advance a context-wide
layout revision. GLFW metrics/content-scale changes already invalidate all levels.
Enabled/focus/hover/paint-only changes reuse layout. Revisions currently invalidate
measurement caches across the context; per-subtree invalidation is future optimization.

Each widget retains at most four constrained measurements. An unchanged root area,
input revision, and geometry revision take a fast path without measurement or
arrangement. Another explicit layout publication invalidates that root shortcut,
avoiding stale geometry when arranging overlapping subtrees independently.
`layoutStats()` reports measurements, cache hits, and arranged widgets for the most
recent explicit measure/layout call. These are diagnostic counters, not frame-time
or idle-CPU performance claims.

Arrangement is staged and validated before publishing any bounds. A measurement
exception, invalid size, bad custom strategy box count, or invalid box leaves all
previous geometry intact. A failed pass can populate valid measurement cache entries;
changing input/strategy must use the normal invalidation contract before retrying.
Application transforms, IDs, focus, and event handlers remain registered.

Custom strategies implement the polymorphic `Layout` interface and receive
ephemeral `LayoutItem` measurement adapters. They must not retain them, mutate
Context, or return a different box count. Returned boxes must honor their children's
size limits and use finite coordinates/nonnegative dimensions.

## Responsive example and tests

```sh
cmake -S . -B build-ui -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON -DVECTORGL_BUILD_EXAMPLES=OFF -DVECTORGL_BUILD_APPS=OFF
cmake --build build-ui --config Debug --target vectorgl_ui_layout vectorgl_ui_layout_tests
ctest --test-dir build-ui -C Debug -R vectorgl_ui_layout --output-on-failure
```

`vectorgl_ui_layout` uses row/column containers and an auto/fraction grid for a
responsive property form. The value surfaces are focusable demonstration data;
editing/validation controls remain a later slice. Narrow windows stack panels using
the same registered tree. Buttons change font size and explanation content to
demonstrate remeasurement. Options: `--font PATH`, `--width N`, `--height N`,
`--smoke-test`, and `--screenshot FILE.bmp`.
Display-only value and grid surfaces keep a steady fill under pointer movement;
action buttons retain hover feedback and keyboard focus remains visible.

For the repository's usual Windows Debug directory, enable the optional modules
when configuring and build the target explicitly:

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_ui_layout
.\build\windows-debug\example\Debug\vectorgl_ui_layout.exe
```

Headless tests cover constraints, flex capping/shrinking, fixed/minimum overflow,
width-dependent height, grid spans, intrinsic sizes, hidden/disabled state, caches,
theme/content invalidation, callback failures, and legacy bounds. GPU tests compare
arranged paint pixels and hits at 100%, 125%, 150%, and 200% scale. Installed consumers
instantiate and run layout through public headers and `vectorgl::ui`.
