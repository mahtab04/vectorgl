#include <iostream>
#include <stdexcept>
#include <vectorgl/ui/native_dialogs.hpp>
using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* text)
{
    if (!value)
        throw std::runtime_error(text);
}
} // namespace
int main()
{
    try
    {
        NativeDialogOptions config;
        config.linuxExecutable = VECTORGL_DIALOG_HELPER;
        auto backend = createNativeFileDialogs(config);
        require(backend->capabilities().open && !backend->capabilities().parented,
                "Linux helper capabilities incorrect");
        FileDialogOptions request;
        request.title = "save";
        request.kind = FileDialogKind::Save;
        request.initialDirectory = "/tmp/a b $(printf forged)";
        request.suggestedName = "quoted'name.svg";
        request.filters = {{"Images", {"svg", "png"}}};
        const auto result = backend->show(request);
        require(result.result.status == ServiceStatus::Success &&
                    result.path == request.initialDirectory + "/" + request.suggestedName,
                "Helper arguments were shell-interpreted/truncated or save flags missing");
        for (const auto& title : {"cancel", "error", "invalid", "nul", "large"})
        {
            request.title = title;
            const auto answer = backend->show(request);
            require(answer.result.status ==
                            (request.title == "cancel" ? ServiceStatus::Cancelled : ServiceStatus::Error) &&
                        answer.path.empty(),
                    "Malformed helper result/cancellation not normalized");
        }
        config.linuxExecutable = "/vectorgl/missing/helper";
        backend = createNativeFileDialogs(config);
        require(!backend->capabilities().open && backend->show({}).result.status == ServiceStatus::Unavailable,
                "Missing executable not unavailable");
        std::cout << "Native helper protocol tests passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
