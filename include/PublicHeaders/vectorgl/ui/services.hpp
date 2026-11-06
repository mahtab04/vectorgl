#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace vectorgl::ui
{
class FileDialogBackend;
enum class ServiceStatus
{
    Success,
    Cancelled,
    Unavailable,
    Error
};
struct ServiceResult
{
    ServiceStatus status = ServiceStatus::Unavailable;
    std::string message;
};
struct ClipboardResult
{
    ServiceResult result;
    std::string text;
};
/*! Injectable clipboard. Cut clients must write successfully before deleting. */
class Clipboard
{
public:
    virtual ~Clipboard() = default;
    virtual ClipboardResult read() = 0;
    virtual ServiceResult write(std::string_view text) = 0;
};
class Clock
{
public:
    virtual ~Clock() = default;
    /*! Finite monotonic seconds; wakeup deadlines use this same clock. */
    virtual double now() const = 0;
};
struct Services
{
    std::shared_ptr<Clock> clock;                   // Null selects std::chrono::steady_clock.
    std::shared_ptr<Clipboard> clipboard;           // Null means unavailable; never touches OS clipboard.
    std::shared_ptr<FileDialogBackend> fileDialogs; // Null means unavailable; fake providers need no window.
};
} // namespace vectorgl::ui
