#include "vectorgl/ui/file_dialogs.hpp"

#include <deque>
#include <stdexcept>

#include "dialog_validation.hpp"

namespace vectorgl::ui
{
struct FileDialogService::State
{
    struct Request
    {
        FileDialogId id;
        FileDialogOptions options;
        WidgetId owner;
        std::function<void(const FileDialogResult&)> completed;
        bool cancelled = false;
    };
    Context& ui;
    std::shared_ptr<FileDialogBackend> backend;
    std::deque<std::shared_ptr<Request>> queue;
    std::shared_ptr<Request> active;
    std::uint64_t next = 1;
    bool pumping = false, closed = false;
    std::function<void()> eventDrain;
    State(Context& c, std::shared_ptr<FileDialogBackend> b)
        : ui(c), backend(b ? std::move(b) : c.services().fileDialogs)
    {
    }
    bool ownerAlive(const Request& r) const
    {
        return !r.owner || ui.isAvailable(r.owner);
    }
};
FileDialogService::FileDialogService(Context& context, std::shared_ptr<FileDialogBackend> backend)
    : state_(std::make_shared<State>(context, std::move(backend)))
{
}
FileDialogService::~FileDialogService()
{
    state_->closed = true;
    state_->queue.clear();
    if (state_->active)
        state_->active->cancelled = true;
}
FileDialogId FileDialogService::request(FileDialogOptions options,
                                        std::function<void(const FileDialogResult&)> completed, WidgetId owner)
{
    if (state_->queue.size() >= 64)
        throw std::length_error("Dialog request queue is full");
    if (!state_->next)
        throw std::overflow_error("Dialog identity exhausted");
    auto request = std::make_shared<State::Request>();
    request->id = {state_->next++};
    request->options = std::move(options);
    request->owner = owner;
    request->completed = std::move(completed);
    state_->queue.push_back(request);
    return request->id;
}
bool FileDialogService::cancel(FileDialogId id)
{
    if (state_->active && state_->active->id == id)
    {
        state_->active->cancelled = true;
        return true;
    }
    for (auto& request : state_->queue)
        if (request->id == id)
        {
            request->cancelled = true;
            return true;
        }
    return false;
}
bool FileDialogService::pending() const
{
    return bool(state_->active) || !state_->queue.empty();
}
FileDialogCapabilities FileDialogService::capabilities() const
{
    return state_->backend ? state_->backend->capabilities() : FileDialogCapabilities{};
}
void FileDialogService::setHostEventDrain(std::function<void()> drain)
{
    state_->eventDrain = std::move(drain);
}
bool FileDialogService::pump()
{
    const auto state = state_; // Completion/backend callbacks may destroy the controller.
    if (state->pumping || state->ui.isProcessing())
        throw std::logic_error("Dialog pump must run outside UI callbacks and cannot reenter");
    if (state->queue.empty())
        return false;
    struct PumpScope
    {
        State& s;
        explicit PumpScope(State& value) : s(value)
        {
            s.pumping = true;
        }
        ~PumpScope()
        {
            s.active.reset();
            s.pumping = false;
        }
    } pump(*state);
    auto request = state->queue.front();
    state->queue.pop_front();
    state->active = request;
    if (!state->ownerAlive(*request))
        return true;
    if (request->owner && state->ui.modalRoot() && !state->ui.isDescendant(request->owner, state->ui.modalRoot()))
        return true;
    FileDialogResult result{{ServiceStatus::Cancelled, {}}, {}};
    if (!request->cancelled)
    {
        if (!detail::dialogOptions(request->options))
            result = detail::dialogError("Invalid file dialog options");
        else if (!state->backend)
            result = {{ServiceStatus::Unavailable, "No file dialog backend is configured"}, {}};
        else
        {
            WidgetId barrier;
            try
            {
                state->ui.cancelCapture(CancelReason::ModalChanged);
                if (state->closed || !state->ownerAlive(*request))
                    return true;
                Widget widget;
                widget.parent = state->ui.modalRoot();
                widget.focusable = true;
                widget.overlay = true;
                barrier = state->ui.add(widget);
                state->ui.pushModal(barrier);
                if (!state->closed && state->ownerAlive(*request) && !request->cancelled)
                    result = detail::normalizedDialogResult(state->backend->show(request->options));
            }
            catch (const std::exception& error)
            {
                result = detail::dialogError(error.what());
            }
            catch (...)
            {
                result = detail::dialogError("File dialog backend failed");
            }
            try
            {
                if (!state->closed && barrier && state->ui.contains(barrier) && state->eventDrain)
                {
                    auto drain = state->eventDrain;
                    drain();
                }
            }
            catch (const std::exception& error)
            {
                result = detail::dialogError(error.what());
            }
            catch (...)
            {
                result = detail::dialogError("Host event drain failed");
            }
            try
            {
                state->ui.remove(barrier);
            }
            catch (const std::exception& error)
            {
                result = detail::dialogError(error.what());
            }
            catch (...)
            {
                result = detail::dialogError("Dialog focus cleanup failed");
            }
        }
    }
    if (request->cancelled)
        result = {{ServiceStatus::Cancelled, {}}, {}};
    if (!state->closed && state->ownerAlive(*request) && request->completed)
    {
        auto callback = std::move(request->completed);
        callback(result); // Focus/modal cleanup and native resource release precede completion.
    }
    return true;
}
} // namespace vectorgl::ui
