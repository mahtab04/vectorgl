#pragma once

#include <functional>
#include <vector>

#include "vectorgl/ui/context.hpp"

namespace vectorgl::ui
{
enum class FileDialogKind
{
    Open,
    Save
};
struct FileFilter
{
    std::string label;
    std::vector<std::string> extensions; // Bare extensions (svg, tar.gz); empty means all files.
};
struct FileDialogOptions
{
    FileDialogKind kind = FileDialogKind::Open;
    std::string title, initialDirectory, suggestedName; // UTF-8. Suggested name is a basename, not a path.
    std::vector<FileFilter> filters;
};
struct FileDialogResult
{
    ServiceResult result;
    std::string path; // Nonempty UTF-8 on Success only. Selection does not read/write the file.
};
struct FileDialogCapabilities
{
    bool open = false, save = false, parented = false;
    std::string backend;
};
/*! Injectable synchronous backend. Called only by FileDialogService::pump on the
 * UI thread, outside Context callbacks. Native backends may block until dismissal. */
class FileDialogBackend
{
public:
    virtual ~FileDialogBackend() = default;
    virtual FileDialogCapabilities capabilities() const = 0;
    virtual FileDialogResult show(const FileDialogOptions& options) = 0;
};
struct FileDialogId
{
    std::uint64_t value = 0;
    explicit operator bool() const
    {
        return value != 0;
    }
    bool operator==(const FileDialogId&) const = default;
};
/*! Queued completion contract. Context must outlive this UI-thread service.
 * request() never enters native UI/calls completion inline. pump() processes one
 * request outside Context callbacks, cancels gestures and establishes a modal
 * barrier, then restores eligible focus before completion. A bound owner that
 * disappears/becomes unavailable suppresses completion. Destruction discards work.
 */
class FileDialogService
{
public:
    explicit FileDialogService(Context& context, std::shared_ptr<FileDialogBackend> backend = {});
    ~FileDialogService();
    FileDialogService(const FileDialogService&) = delete;
    FileDialogService& operator=(const FileDialogService&) = delete;
    FileDialogId request(FileDialogOptions options, std::function<void(const FileDialogResult&)> completed,
                         WidgetId owner = {});
    bool cancel(FileDialogId id); // Queued: skip backend. Active: suppress a late result after native dismissal.
    bool pending() const;
    bool pump(); // False when no request; throws on reentry or during Context processing.
    FileDialogCapabilities capabilities() const;
    // Drain native events while the temporary modal barrier is still active.
    // Blocking helper hosts should install this (e.g. glfwPollEvents).
    void setHostEventDrain(std::function<void()> drain);

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace vectorgl::ui
