# File dialog services

`FileDialogService` belongs to `vectorgl::ui`. Its injectable backend and queued
completion contract work without a native desktop. The optional `vectorgl::dialogs`
component supplies Windows Common Item Dialog and Linux Zenity implementations.
Selecting a path never reads or writes a document.

## Build and run

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_DIALOGS=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_ui_editor_workspace
.\build\windows-debug\example\Debug\vectorgl_ui_editor_workspace.exe
```

Open selects an existing file. Save / close opens the in-app Save/Discard/Cancel
prompt; Save then selects a destination. The status line reports the result and
the application serialization hook. This example intentionally does not implement
a document format or mark a document saved merely because a path was selected.
Smoke mode injects a fake dialog backend and fake clipboard, including error and
cancellation results; it opens no OS dialogs.

Native dialogs default ON alongside UI and the GLFW adapter. They require UI,
but can also be built with the adapter explicitly disabled.
Installed consumers use:

```cmake
find_package(vectorgl CONFIG REQUIRED COMPONENTS dialogs)
target_link_libraries(my_editor PRIVATE vectorgl::dialogs)
```

This imports UI and the renderer, without importing GLFW. Request the `glfw`
component separately if needed. Linking `vectorgl::dialogs` supplies
`VECTORGL_HAS_NATIVE_DIALOGS=1`; its public header is installed only when enabled.
Base/UI discovery does not load this optional component.

## Queuing and lifetime

```cpp
#include <vectorgl/ui/file_dialogs.hpp>
#include <vectorgl/ui/native_dialogs.hpp>

// ui and host already exist; all calls run on their UI thread.
vectorgl::ui::NativeDialogOptions native;
native.ownerWindow = host.nativeDialogOwner();
vectorgl::ui::FileDialogService files(ui, vectorgl::ui::createNativeFileDialogs(native));
files.setHostEventDrain([] { glfwPollEvents(); });

// A button callback enqueues work. No native UI or completion runs inline.
vectorgl::ui::FileDialogOptions options;
options.title = "Open a document";
options.filters = {{"Documents", {"svg", "json"}}, {"All files", {}}};
auto request = files.request(options, [](const auto& answer) {
    if (answer.result.status == vectorgl::ui::ServiceStatus::Success) {
        // Application: validate/load answer.path; report any I/O failure.
    }
});

// Host loop, outside dispatch, layout, painting, or timer callbacks:
if (files.pending())
    files.pump(); // One queued request; a native backend can block until dismissal.
```

Destroy the service before Context, and both before the host window. A service
also accepts `Services::fileDialogs` when no constructor backend is supplied.
Bind a `WidgetId` as the third request argument to suppress completion when that
owner is removed, hidden, or disabled. Requests outside an active modal owner's
scope are discarded. Completion can enqueue more work; recursive `pump()` and
pumping during a Context transaction throw `std::logic_error`.

Before entering native code the service cancels pointer capture/drag gestures and
pushes a temporary modal scope. It drains host events before removing that scope,
so input queued during a blocking helper cannot click through to the application.
Install `setHostEventDrain()` for such hosts, forwarding events normally through
the adapter. Eligible previous focus is restored before completion; removed,
hidden or disabled owners are revalidated rather than receiving stale focus.

`cancel(id)` skips a queued backend call and later completes with Cancelled. For
an active blocking backend it suppresses a late success; it **does not close the
OS dialog**. The user must dismiss it before the pump returns. Destruction drops
queued work and suppresses active completion. Context itself must remain alive.
The queue is bounded to 64 waiting requests; overflow throws `std::length_error`.

## Result and path contract

| Status | Meaning |
|---|---|
| Success | A nonempty UTF-8 path was selected; application I/O still needs validation. |
| Cancelled | User dismissed the chooser or the request was cancelled. |
| Unavailable | No configured backend/helper, unsupported platform, or incompatible Windows COM apartment. |
| Error | Invalid options, backend failure, malformed output, or host cleanup failure. |

Only Success carries a path. Backend exceptions become Error; completion exceptions
propagate after native resources/modal state have been cleaned up. Capabilities
describe backend support, not a guarantee that a desktop session will accept a
dialog. Applications decide how to display errors or offer a typed-path fallback.

Options/results reject malformed UTF-8, NULs, and strings over 32 KiB. Suggested
names are portable basenames, not paths. Filters use bare extensions (`svg`,
`tar.gz`), with an empty extension list meaning all files. Labels cannot contain
control characters or `|`; extensions contain ASCII letters/digits, `.`, `_`, `-`,
without leading/trailing dots. At most 32 filters and 32 extensions per filter are
accepted. No extension is appended after native confirmation, which could bypass
the chooser's overwrite check. Applications should select a suitable basename.

## Native backends

Windows uses the [Common Item Dialog](https://learn.microsoft.com/en-us/windows/win32/shell/common-file-dialog)
with an STA UI-thread apartment, filesystem paths, existing-file checks on Open,
and overwrite prompts on Save. It uses RAII for COM objects, apartment lifetime,
and selected-path memory. `GlfwAdapter::nativeDialogOwner()` supplies the HWND;
custom hosts can supply their own valid HWND as `uintptr_t`. The backend does not
change an existing incompatible COM apartment to STA.

Linux launches runtime-optional `zenity` using `posix_spawn` with individual argv
entries, never a shell. There is no Qt, GTK, D-Bus or portal link dependency in
VectorGL; the installed helper has its own desktop dependencies. Configure
`NativeDialogOptions::linuxExecutable` for an explicit executable path. Missing
helpers report Unavailable. Exit 1 means cancellation; other failing exits and
malformed/oversized output report Error. Filename whitespace is preserved.
See GNOME's [file-selection documentation](https://gitlab.gnome.org/GNOME/zenity/-/blob/master/help/C/file-selection.page).

Linux currently reports `parented=false`: the helper does not establish native
window ownership or guarantee application-wide OS modality. Context routing still
uses its modal barrier; applications with other windows must manage their own
window policy. This is not a portal/sandbox integration. Real X11/Wayland session
checks remain required before a consumer declares Linux desktop support.
