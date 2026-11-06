# Migrating an application to VectorGL UI

The optional UI modules provide a reusable workspace foundation without Qt
headers, moc, QML, or Qt runtime dependencies. Replacing an application's Qt usage
also requires migrating its domain model, persistence, tooling and platform
requirements. The standalone workspace exercises public APIs; it does not prove
compatibility with another application's project format or element types.

## Components and ownership

| Installed component | Use |
|---|---|
| Base `vectorgl::vectorgl` | Canvas, renderer, text, scene and picking; no UI/host discovery. |
| `ui` / `vectorgl::ui` | Widget identity/routing, layout, controls, overlays, collections, typed in-process drag/drop, injectable services. |
| `glfw` / `vectorgl::glfw` | Explicit event forwarding, DPI coordinates, cursor/clipboard host services; imports UI and GLFW. |
| `dialogs` / `vectorgl::dialogs` | Optional Windows/Linux file chooser backends; imports UI without GLFW. |

Enable `VECTORGL_BUILD_UI`, then the host/dialog options you need. Keep the native
window and GL context alive until all UI painters, fonts, renderer assets and
adapters are destroyed. `Context` must outlive controls and FileDialogService.
Detach callbacks before their targets are destroyed. The host adapter never takes
over the application's window user pointer or installs callbacks implicitly.

## Migration sequence

1. Build the [installed workspace consumer](ui-editor-workspace.md) against a
   staging SDK. Choose the supported OS/session, locales, accessibility and native
   service requirements before committing to a release.
2. Replace the host shell with explicit GLFW/custom-host forwarding. Forward
   pointer, keyboard, committed characters, focus, iconification, size and scale
   changes. Use the same inverse transforms for rendering and picking. See
   [host integration](ui-glfw.md) and the example's callback scope.
3. Compose panels using [layout](ui-layout.md), [containers](ui-containers.md),
   [controls](ui-controls.md), [overlays](ui-overlays.md) and
   [workspace controls](ui-workspace-controls.md). Use stable model IDs in
   [lists](ui-lists.md), [trees](ui-trees.md) and [tables](ui-tables.md).
4. Keep the artboard Scene separate from UI layout. Use
   [owned typed drag/drop](ui-drag-drop.md) for toolbox placement. Document undo,
   serialization, validation and domain element semantics remain application code.
5. Queue [file dialogs](ui-file-dialogs.md) from controls; pump them outside Context
   transactions. On success perform application I/O and only then update the dirty
   state. Handle Cancelled, Unavailable and Error separately. Test with fake dialog
   and clipboard providers.
6. Retain source-application compatibility tests while replacing panels/features
   incrementally. Remove the remaining Qt build/runtime dependencies only after
   those tests and the platform gates below pass.

Qt signals/models do not have direct binary equivalents. Bind UI callbacks to the
application's existing model and expose visible snapshots with stable identities.
For filtered lists/tables use `setProjection()` with the complete enabled-ID set;
otherwise a missing ID means removal. Tree collapse preserves selection. Use
Context focus/capture/modal APIs instead of independent per-widget event loops.

## Host and release gates

The event loop prepares/paints only after invalidation, wakes for UI timer
deadlines, and pumps pending file requests before entering idle waits. Minimized
or nondrawable hosts cancel capture and focus; application callbacks explicitly
forward iconification to the adapter. Windows native dialogs use a parent HWND;
the Linux Zenity backend currently has no native parent linkage. Rendering into
embedded nonzero framebuffers follows the renderer's framebuffer/viewport contract.

| Check | Status for this delivery |
|---|---|
| Headless routing, service queue, fake provider statuses/lifetimes and gesture cancellation | Automated regression coverage. |
| GPU painting, DPI, overlays, compact/wide workspace and injected dialog scenarios | Included in the local Windows regression run. |
| Windows MSVC and MinGW build; installed UI/host/dialog consumer | Local validation; see delivery test results. |
| Graphics-only, UI-only, dialogs without GLFW | Package isolation checks; native options default OFF. |
| Linux GCC Debug/Release, sanitizer, installed consumers, helper protocol and Clang quality | CI configured; not executed on this Windows development host. |
| Real Windows native chooser interaction | Open success and Escape cancellation; Save returned a Unicode destination without writing a file; eligible focus restored. |
| Real Linux native chooser interaction | Pending X11/Wayland desktop checks; mocked tests do not establish parity. |
| Consumer's domain element types and saved-project compatibility | Consumer-owned tests, required before migration release. |

Supported editing remains left-to-right, Unicode codepoint based. Grapheme-safe
editing, shaping/bidi/fallback, IME composition, multiline editor behavior, and OS
accessibility bridges remain separate work. Committed GLFW character events do
not implement preedit/composition. Local editor undo/word navigation and document
undo also require their own design. Keyboard navigation alone does not provide
screen-reader accessibility. Consumers requiring these capabilities must keep
them as release gates; this delivery does not establish universal Qt parity.

External/cross-process drag/drop, portal dialogs, persistent docking and domain
serialization are outside the current reusable workspace APIs. Performance
comparisons should record hardware, driver, build mode, font, scale, workload,
visible-row counts and steady-state timing; the existing smoke baseline is CPU
submission timing, not a GPU/FPS promise.
