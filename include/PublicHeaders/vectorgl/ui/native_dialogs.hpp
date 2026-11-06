#pragma once

#include "vectorgl/ui/file_dialogs.hpp"

namespace vectorgl::ui
{
struct NativeDialogOptions
{
    std::uintptr_t ownerWindow = 0;         // Windows HWND from GlfwAdapter::nativeDialogOwner(), or zero.
    std::string linuxExecutable = "zenity"; // Executable/path, never a shell command.
};
/*! Optional vectorgl::dialogs component. Windows Common Item Dialog (STA UI
 * thread), Linux Zenity process backend. Other platforms report Unavailable.
 * The Linux helper is runtime-optional; no GTK/DBus/Qt link dependency is added. */
std::shared_ptr<FileDialogBackend> createNativeFileDialogs(NativeDialogOptions options = {});
} // namespace vectorgl::ui
