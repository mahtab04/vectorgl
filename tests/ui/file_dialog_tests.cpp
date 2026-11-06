#include <iostream>
#include <stdexcept>
#include <vectorgl/ui/file_dialogs.hpp>
using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* text)
{
    if (!value)
        throw std::runtime_error(text);
}
class Fake final : public FileDialogBackend
{
public:
    int calls = 0;
    FileDialogResult result{{ServiceStatus::Success, {}}, "/tmp/owned \xCE\xA9.svg"};
    std::function<void()> during;
    FileDialogCapabilities capabilities() const override
    {
        return {true, true, false, "fake"};
    }
    FileDialogResult show(const FileDialogOptions&) override
    {
        ++calls;
        if (during)
            during();
        return result;
    }
};
WidgetId widget(Context& ui)
{
    Widget w;
    w.bounds = {0, 0, 100, 100};
    w.focusable = true;
    return ui.add(w);
}
void resultsAndValidation()
{
    Context ui;
    auto fake = std::make_shared<Fake>();
    FileDialogService service(ui, fake);
    const auto owner = widget(ui);
    ui.focus(owner);
    int completions = 0;
    FileDialogResult received;
    auto callback = [&](const FileDialogResult& value)
    {
        require(!ui.modalRoot() && ui.focused() == owner, "Completion preceded modal/focus cleanup");
        received = value;
        ++completions;
    };
    fake->during = [&] { require(bool(ui.modalRoot()) && !ui.captured(), "Backend entered without modal barrier"); };
    service.request({}, callback, owner);
    require(completions == 0 && fake->calls == 0 && service.pending(), "Request completed inline");
    require(service.pump() && completions == 1 && received.result.status == ServiceStatus::Success &&
                received.path.find("owned") != std::string::npos,
            "Success result failed");
    for (auto status : {ServiceStatus::Cancelled, ServiceStatus::Unavailable, ServiceStatus::Error})
    {
        fake->result = {{status, "detail"}, "must not escape"};
        service.request({}, callback, owner);
        service.pump();
        require(received.result.status == status && received.path.empty(), "Non-success leaked path");
    }
    const auto count = fake->calls;
    FileDialogOptions invalid;
    invalid.title = std::string("bad\0title", 9);
    service.request(invalid, callback, owner);
    service.pump();
    require(fake->calls == count && received.result.status == ServiceStatus::Error, "NUL options reached backend");
    invalid = {};
    invalid.filters = {{"Images", {"png;*.exe"}}};
    service.request(invalid, callback);
    service.pump();
    require(fake->calls == count && received.result.status == ServiceStatus::Error, "Invalid filters reached backend");
    fake->result = {{ServiceStatus::Success, {}}, std::string("\xff")};
    service.request({}, callback);
    service.pump();
    require(received.result.status == ServiceStatus::Error && received.path.empty(), "Invalid path accepted");
    fake->during = [] { throw std::runtime_error("backend failure"); };
    service.request({}, callback);
    service.pump();
    require(received.result.status == ServiceStatus::Error && !ui.modalRoot(), "Throwing backend retained modal");
    require(!service.pending() && !service.pump(), "Service did not become idle");
    FileDialogService unavailable(ui);
    unavailable.request({}, callback);
    unavailable.pump();
    require(received.result.status == ServiceStatus::Unavailable, "Missing backend did not report unavailable");
}
void cancellationAndLifetimes()
{
    Context ui;
    auto fake = std::make_shared<Fake>();
    FileDialogService service(ui, fake);
    const auto owner = widget(ui);
    int completions = 0;
    const auto cancelled = service.request(
        {},
        [&](const auto& r)
        {
            ++completions;
            require(r.result.status == ServiceStatus::Cancelled, "Queued cancel returned success");
        },
        owner);
    require(service.cancel(cancelled), "Cancel did not find queued request");
    service.pump();
    require(fake->calls == 0 && completions == 1 && !service.cancel(cancelled),
            "Cancelled request entered backend/repeated");
    const auto id = service.request(
        {},
        [&](const auto& result)
        {
            ++completions;
            require(result.result.status == ServiceStatus::Cancelled && result.path.empty(),
                    "Active cancel delivered a late success/path");
        },
        owner);
    fake->during = [&] { service.cancel(id); };
    service.pump();
    require(completions == 2 && !ui.modalRoot(), "Active cancellation failed cleanup");
    service.request({}, [&](const auto&) { ++completions; }, owner);
    ui.remove(owner);
    service.pump();
    require(completions == 2 && fake->calls == 1, "Removed queued owner received callback/backend");
    auto next = widget(ui);
    service.request({}, [&](const auto&) { ++completions; }, next);
    fake->during = [&] { ui.remove(next); };
    service.pump();
    require(completions == 2 && !ui.modalRoot(), "Owner removed during backend received stale completion");
    fake->during = {};
    service.request({}, [](const auto&) { throw std::runtime_error("completion failure"); });
    bool threw = false;
    try
    {
        service.pump();
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    require(threw && !service.pending() && !ui.modalRoot(), "Throwing completion retained busy/modal state");
    std::unique_ptr<FileDialogService> dying = std::make_unique<FileDialogService>(ui, fake);
    fake->during = [&] { dying.reset(); };
    dying->request({}, [&](const auto&) { ++completions; });
    dying->pump();
    require(!dying && completions == 2 && !ui.modalRoot(), "Destruction during backend invoked stale callback");
}
void inputAndReentry()
{
    Context ui;
    auto fake = std::make_shared<Fake>();
    FileDialogService service(ui, fake);
    int cancelled = 0;
    Widget source;
    source.bounds = {0, 0, 100, 100};
    source.onEvent = [&](Context& c, WidgetId id, const Event& e)
    {
        if (e.type == EventType::PointerDown)
        {
            c.beginDrag(id, DragPayload::make(42), [&](const auto&) { ++cancelled; });
            service.request({}, [](const auto&) {});
            bool threw = false;
            try
            {
                service.pump();
            }
            catch (const std::logic_error&)
            {
                threw = true;
            }
            require(threw, "Pump reentered UI callback");
        }
        return EventReply{true};
    };
    ui.add(source);
    Event event;
    event.type = EventType::PointerDown;
    event.x = 5;
    event.y = 5;
    event.button = PointerButton::Primary;
    ui.dispatch(event);
    require(bool(ui.dragState()), "Test drag did not start");
    fake->during = [&]
    {
        require(!ui.dragState() && !ui.captured() && cancelled == 1, "Dialog did not cancel drag before entry");
        bool threw = false;
        try
        {
            service.pump();
        }
        catch (const std::logic_error&)
        {
            threw = true;
        }
        require(threw, "Backend reentered pump");
        require(ui.dispatch(event), "Native modal barrier allowed application click");
    };
    service.setHostEventDrain(
        [&]
        {
            require(bool(ui.modalRoot()), "Host drain ran after native barrier removal");
            require(ui.dispatch(event), "Queued native event leaked behind dialog");
        });
    service.pump();
    require(!ui.modalRoot() && cancelled == 1, "Dialog input cleanup failed");
}
void nestedModalAndDrainFailure()
{
    Context ui;
    auto fake = std::make_shared<Fake>();
    FileDialogService service(ui, fake);
    const auto previous = widget(ui);
    ui.focus(previous);
    const auto modal = widget(ui);
    ui.pushModal(modal);
    FileDialogResult result;
    service.setHostEventDrain([] { throw std::runtime_error("drain failure"); });
    service.request({}, [&](const auto& r) { result = r; }, modal);
    service.pump();
    require(ui.modalRoot() == modal && ui.focused() == modal && result.result.status == ServiceStatus::Error,
            "Drain failure removed enclosing modal or failed to report error");
    service.setHostEventDrain({});
    int completions = 0;
    fake->during = [&] { ui.setEnabled(modal, false); };
    service.request({}, [&](const auto&) { ++completions; }, modal);
    service.pump();
    require(completions == 0 && !ui.modalRoot() && ui.focused() == previous,
            "Unavailable owner received completion or stale modal focus");
}
} // namespace
int main()
{
    try
    {
        resultsAndValidation();
        cancellationAndLifetimes();
        inputAndReentry();
        nestedModalAndDrainFailure();
        std::cout << "File dialog tests passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
