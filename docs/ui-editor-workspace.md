# Integrated standalone workspace — roadmap milestone 10

This delivery follows row 10 of the supplied reusable-UI plan. Roadmap row 9's
TreeView and TableView were split into separate implementation PRs; the earlier
table implementation called “PR 10” did not implement this drag/drop milestone.
The roadmap itself is not included in this change.

`example/ui_editor_workspace_demo.cpp` combines the optional public UI APIs with a
GLFW host and a separate Scene artboard. It has four regions joined by three
constrained splitters: searchable toolbox, artboard, inspector/hierarchy tabs, and
a searchable 2,000-row tag table. Property controls include a routed text field,
numeric editor, checkbox, collapsible section, and symbolic ComboBox. Save/Discard/
Cancel returns an explicit result; persistence remains an application hook.

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_ui_editor_workspace
.\build\windows-debug\example\Debug\vectorgl_ui_editor_workspace.exe
```

The example accepts `--width`, `--height`, `--font PATH`, `--smoke-test`, and
`--screenshot FILE.bmp`. Defaults are 1680×980. The planned compact size is 1280×780.
Drag a toolbox entry, or press Enter/use Place selected for keyboard placement.
The last entry demonstrates rejection. Ctrl+wheel and toolbar zoom affect the
artboard only; scene placement and picking use the same UI/DPI/zoom mapping.
Delete removes the selected scene object only while the artboard owns focus.
Name/opacity/enabled properties modify the selected in-memory object.

## Selection correction required by the plan

The previous collection slices interpreted an absent snapshot ID as removal and
tree collapse discarded selection. The supplied plan instead requires retaining
hidden IDs until they are actually removed. Lists/tables now expose
`setProjection(visibleModel, completeEnabledIds)` so filtering and removal are
explicitly distinguishable. Current/range focus stays visible; hidden selection
does not move it to an invisible widget. Tree collapse retains existing enabled
descendant selection automatically. Replacement of the full model still removes
deleted/disabled IDs. The existing collection examples now use this filtering
contract and their tests/documentation are updated.

## Acceptance checks

| Planned gate | Implementation and verification |
|---|---|
| Four resizable regions, 1680×980 and 1280×780 | Both sizes have executable smoke tests and screenshot inspection; fixed search/header sizing keeps controls reachable. |
| Toolbox, property form, symbolic popup, hierarchy, table | Composed using installed-public UI headers; no application-private includes or Qt types. |
| Typed drops, feedback, rejection, cancellation | Context state-machine tests plus workspace accepted/rejected/unsupported, Escape, and active-drag focus-loss scenarios. |
| Separate artboard zoom | Workspace smoke checks unchanged toolbox bounds and inverse-zoom scene placement; rendering/picking share one scene transform. |
| Editing, modal barrier, keyboard-only operations | Delete consumption, Tab traversal, Enter placement, existing routed-control tests, Save/Discard/Cancel and Escape. |
| DPI, active field/popup, nonuniform ratios | Workspace smoke renders nonzero RGBA/stencil FBOs at 1×/1.25×/1.5×/2× and 1.25×/1.5× axes, changes scale with a field/popup active, reads a scene pixel, checks host coordinate mapping and FBO preservation. |
| 2,000 rows and stable selection | Existing visible-cell/row counters and GPU tests; workspace filter/reorder/selection scenarios, tree-collapse retention, and deletion validation in collection tests. |
| Idle invalidation and measured baseline | No repeated layout invalidation or orphan caret timer after focus is cleared. Smoke mode samples idle process CPU over ~50 ms and reports frame submission time, draw calls/uploads, framebuffer workload, and GPU renderer. These are observations, not FPS budgets. |
| Installed UI+host, UI-only, graphics-only | Installed workspace is a separate CMake consumer option; existing component-isolation consumers remain. Matching public GLAD/KHR headers and loader license are now installed. |
| Linux headless/sanitizers and Windows/Linux hosts | Existing CI builds include the new headless tests and workspace; installed workspace execution is gated to GPU/display jobs. Local Windows/MinGW and MSVC checks are recorded separately; Linux CI results must be checked before calling the full cross-platform gate passed. |

The smoke baseline measures CPU-side frame submission, not GPU elapsed time or
steady-state interactive FPS. The short idle sample is coarse on platforms with
low-resolution process-time counters. Record hardware/driver, build mode, font,
window size, and workload when comparing runs. Do not assign a performance budget
from a single sample or compare fixture-font and system-font runs as equivalent.

Local observed baseline: Windows, MinGW Debug, Segoe UI at 16 logical pixels,
Intel Arc 130T GPU (16 GB), CPU identifier Intel64 Family 6 Model 197 Stepping 2.
Each sample includes one native frame, four DPI frames (with field/popup state),
and one nonuniform-DPI frame; font/cache warmup is included. Idle sampling follows
with focus cleared. The draw/upload counters below are the last nonuniform frame.

| Logical workspace | Visible tag rows | Mean CPU submission (ms) | Draw calls | Upload bytes | Idle process CPU / sampled wall (ms) |
|---|---:|---:|---:|---:|---:|
| 1680×980 | 5 | 42.63 | 187 | 385,668 | 0 / 54.44 |
| 1280×780 | 3 | 39.87 | 163 | 361,500 | 0 / 60.67 |

Zero in these short idle CPU samples reflects process-counter granularity, not a
guarantee of zero resource use. These cold smoke measurements establish a baseline;
they are not a steady-state performance target or a claim about other hardware.

Local validation passed: all 46 engine/UI/GPU/example tests, five targeted MSVC
tests, installed UI+host/workspace consumers with MinGW and MSVC, UI-only imported
consumer, and a freshly built graphics-only package/consumer without GLFW fetching.
Linux jobs are wired in CI but were not executed locally on this Windows host.

## Installed workspace consumer

After installing the SDK to `staging`:

```sh
cmake -S tests/ui_package_consumer -B installed-workspace -DCMAKE_PREFIX_PATH=/absolute/path/staging -DVECTORGL_TEST_GLFW=ON -DVECTORGL_TEST_WORKSPACE=ON -DVECTORGL_TEST_WORKSPACE_GPU=ON
cmake --build installed-workspace --config Debug
ctest --test-dir installed-workspace -C Debug --output-on-failure
```

The workspace source compiles against imported `vectorgl::glfw` and installed
headers. No build-tree include paths or example-private headers are provided.
Set the GPU option OFF to compile without running a graphical test. UI-only/base
consumer configurations remain separate and do not import the host accidentally.

## Remaining release work

Roadmap milestone 11 remains: Windows/Linux file-dialog services, remaining host
hardening, platform matrix results, and migration guidance. This sample's Save
action reports an application request; it does not provide native file dialogs or
a document format. Linux interactive/native-dialog validation is not implied by
Windows smoke tests. Full shaping, IME, grapheme editing, and accessibility remain
the plan's later/consumer-dependent release gates. This delivery does not establish
universal Qt parity or verify HMI Designer's domain implementation.
