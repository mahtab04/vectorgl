#include "vectorgl/ui/native_dialogs.hpp"

#include <filesystem>
#include <stdexcept>
#include <system_error>

#include "dialog_validation.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shobjidl.h>
#include <windows.h>
#elif defined(__linux__)
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace vectorgl::ui
{
namespace
{
#ifdef _WIN32
template <class T> struct ComObject
{
    T* value = nullptr;
    ~ComObject()
    {
        if (value)
            value->Release();
    }
    ComObject() = default;
    ComObject(const ComObject&) = delete;
    ComObject& operator=(const ComObject&) = delete;
};
std::wstring wide(std::string_view text)
{
    if (text.empty())
        return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (!size)
        throw std::runtime_error("Cannot convert dialog text to UTF-16");
    std::wstring result(std::size_t(size), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), result.data(), size))
        throw std::runtime_error("Cannot convert dialog text to UTF-16");
    return result;
}
std::string utf8(const wchar_t* text)
{
    if (!text)
        throw std::runtime_error("Native dialog returned no path");
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1 || size > 32769)
        throw std::runtime_error("Native dialog returned an invalid/oversized path");
    std::string result(std::size_t(size), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result.data(), size, nullptr, nullptr))
        throw std::runtime_error("Cannot convert dialog path to UTF-8");
    result.pop_back();
    return result;
}
FileDialogResult hresult(HRESULT hr)
{
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return {{ServiceStatus::Cancelled, {}}, {}};
    return detail::dialogError("Windows file dialog failed (HRESULT " + std::to_string(static_cast<unsigned long>(hr)) +
                               ")");
}
class NativeBackend final : public FileDialogBackend
{
    NativeDialogOptions options_;

public:
    explicit NativeBackend(NativeDialogOptions value) : options_(std::move(value)) {}
    FileDialogCapabilities capabilities() const override
    {
        return {true, true, options_.ownerWindow != 0, "Windows Common Item Dialog"};
    }
    FileDialogResult show(const FileDialogOptions& options) override
    {
        if (!detail::dialogOptions(options))
            return detail::dialogError("Invalid file dialog options");
        const auto owner = reinterpret_cast<HWND>(options_.ownerWindow);
        if (owner && !IsWindow(owner))
            return detail::dialogError("Native dialog owner window is no longer valid");
        const auto init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (init == RPC_E_CHANGED_MODE)
            return {{ServiceStatus::Unavailable, "Windows file dialogs require an STA UI thread"}, {}};
        if (FAILED(init))
            return hresult(init);
        struct Apartment
        {
            ~Apartment()
            {
                CoUninitialize();
            }
        } apartment;
        ComObject<IFileDialog> dialog;
        auto hr =
            CoCreateInstance(options.kind == FileDialogKind::Open ? CLSID_FileOpenDialog : CLSID_FileSaveDialog,
                             nullptr, CLSCTX_INPROC_SERVER, IID_IFileDialog, reinterpret_cast<void**>(&dialog.value));
        if (FAILED(hr))
            return hresult(hr);
        DWORD flags = 0;
        hr = dialog.value->GetOptions(&flags);
        if (FAILED(hr))
            return hresult(hr);
        flags |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_PATHMUSTEXIST | FOS_DONTADDTORECENT;
        flags |= options.kind == FileDialogKind::Open ? FOS_FILEMUSTEXIST : FOS_OVERWRITEPROMPT;
        hr = dialog.value->SetOptions(flags);
        if (FAILED(hr))
            return hresult(hr);
        const auto title = wide(options.title), name = wide(options.suggestedName);
        if (!title.empty())
        {
            hr = dialog.value->SetTitle(title.c_str());
            if (FAILED(hr))
                return hresult(hr);
        }
        if (!name.empty())
        {
            hr = dialog.value->SetFileName(name.c_str());
            if (FAILED(hr))
                return hresult(hr);
        }
        if (!options.initialDirectory.empty())
        {
            ComObject<IShellItem> folder;
            const auto path = wide(options.initialDirectory);
            hr = SHCreateItemFromParsingName(path.c_str(), nullptr, IID_IShellItem,
                                             reinterpret_cast<void**>(&folder.value));
            if (FAILED(hr))
                return hresult(hr);
            hr = dialog.value->SetDefaultFolder(folder.value);
            if (FAILED(hr))
                return hresult(hr);
        }
        std::vector<std::wstring> labels, patterns;
        for (const auto& filter : options.filters)
        {
            labels.push_back(wide(filter.label));
            std::string pattern;
            for (const auto& ext : filter.extensions)
            {
                if (!pattern.empty())
                    pattern += ';';
                pattern += "*." + ext;
            }
            patterns.push_back(wide(pattern.empty() ? "*.*" : pattern));
        }
        std::vector<COMDLG_FILTERSPEC> filters;
        for (std::size_t i = 0; i < labels.size(); ++i)
            filters.push_back({labels[i].c_str(), patterns[i].c_str()});
        if (!filters.empty())
        {
            hr = dialog.value->SetFileTypes(UINT(filters.size()), filters.data());
            if (FAILED(hr))
                return hresult(hr);
        }
        hr = dialog.value->Show(owner);
        if (FAILED(hr))
            return hresult(hr);
        ComObject<IShellItem> selected;
        hr = dialog.value->GetResult(&selected.value);
        if (FAILED(hr))
            return hresult(hr);
        struct Path
        {
            PWSTR value = nullptr;
            ~Path()
            {
                CoTaskMemFree(value);
            }
        } path;
        hr = selected.value->GetDisplayName(SIGDN_FILESYSPATH, &path.value);
        if (FAILED(hr))
            return hresult(hr);
        return detail::normalizedDialogResult({{ServiceStatus::Success, {}}, utf8(path.value)});
    }
};
#elif defined(__linux__)
std::string executable(std::string name)
{
    if (name.empty() || name.find('\0') != std::string::npos)
        return {};
    const auto usable = [](const std::string& path)
    {
        std::error_code error;
        return access(path.c_str(), X_OK) == 0 && std::filesystem::is_regular_file(path, error);
    };
    if (name.find('/') != std::string::npos)
        return usable(name) ? name : std::string{};
    const auto* env = std::getenv("PATH");
    if (!env)
        return {};
    const std::string paths = env;
    for (std::size_t start = 0; start <= paths.size();)
    {
        const auto end = paths.find(':', start);
        const auto directory = paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
        // Do not implicitly search the current directory through empty PATH entries.
        if (!directory.empty())
        {
            auto path = directory + "/" + name;
            if (usable(path))
                return path;
        }
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return {};
}
struct Child
{
    Child() = default;
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;
    pid_t pid = -1;
    int read = -1, write = -1;
    ~Child()
    {
        if (read >= 0)
            close(read);
        if (write >= 0)
            close(write);
        if (pid > 0)
        {
            kill(pid, SIGKILL);
            while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR)
            {
            }
        }
    }
};
class NativeBackend final : public FileDialogBackend
{
    NativeDialogOptions options_;

public:
    explicit NativeBackend(NativeDialogOptions value) : options_(std::move(value)) {}
    FileDialogCapabilities capabilities() const override
    {
        const bool found = !executable(options_.linuxExecutable).empty();
        return {found, found, false, "Linux Zenity"};
    }
    FileDialogResult show(const FileDialogOptions& options) override
    {
        if (!detail::dialogOptions(options))
            return detail::dialogError("Invalid file dialog options");
        const auto binary = executable(options_.linuxExecutable);
        if (binary.empty())
            return {{ServiceStatus::Unavailable, "Zenity executable is unavailable"}, {}};
        std::vector<std::string> arguments{binary, "--file-selection", "--title=" + options.title};
        if (options.kind == FileDialogKind::Save)
        {
            arguments.push_back("--save");
            arguments.push_back("--confirm-overwrite");
        }
        std::string initial = options.initialDirectory;
        if (!initial.empty() && initial.back() != '/')
            initial += '/';
        initial += options.suggestedName;
        if (!initial.empty())
            arguments.push_back("--filename=" + initial);
        for (const auto& filter : options.filters)
        {
            std::string value = "--file-filter=" + filter.label + " |";
            if (filter.extensions.empty())
                value += " *";
            for (const auto& ext : filter.extensions)
                value += " *." + ext;
            arguments.push_back(std::move(value));
        }
        std::vector<char*> argv;
        for (auto& arg : arguments)
            argv.push_back(arg.data());
        argv.push_back(nullptr);
        Child child;
        int pipes[2];
        if (pipe(pipes) < 0)
            return detail::dialogError("Cannot create dialog result pipe");
        child.read = pipes[0];
        child.write = pipes[1];
        // Keep file actions independent of closed/reused stdin/stdout/stderr.
        for (auto* fd : {&child.read, &child.write})
        {
            if (*fd >= STDERR_FILENO + 1)
                continue;
            const auto relocated = fcntl(*fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
            if (relocated < 0)
                return detail::dialogError("Cannot relocate dialog file descriptors");
            close(*fd);
            *fd = relocated;
        }
        if (fcntl(child.read, F_SETFD, FD_CLOEXEC) < 0 || fcntl(child.write, F_SETFD, FD_CLOEXEC) < 0)
            return detail::dialogError("Cannot protect dialog file descriptors");
        struct Actions
        {
            Actions() = default;
            Actions(const Actions&) = delete;
            Actions& operator=(const Actions&) = delete;
            posix_spawn_file_actions_t value;
            bool initialized = false;
            ~Actions()
            {
                if (initialized)
                    posix_spawn_file_actions_destroy(&value);
            }
        } actions;
        int error = posix_spawn_file_actions_init(&actions.value);
        if (error)
            return detail::dialogError("Cannot initialize dialog process");
        actions.initialized = true;
        error = posix_spawn_file_actions_addclose(&actions.value, child.read);
        if (!error)
            error = posix_spawn_file_actions_adddup2(&actions.value, child.write, STDOUT_FILENO);
        if (!error && child.write != STDOUT_FILENO)
            error = posix_spawn_file_actions_addclose(&actions.value, child.write);
        if (error)
            return detail::dialogError("Cannot configure dialog process");
        error = posix_spawn(&child.pid, binary.c_str(), &actions.value, nullptr, argv.data(), environ);
        if (error)
        {
            child.pid = -1;
            return {{error == ENOENT ? ServiceStatus::Unavailable : ServiceStatus::Error,
                     "Cannot launch Zenity (errno " + std::to_string(error) + ")"},
                    {}};
        }
        close(child.write);
        child.write = -1;
        std::string output;
        char buffer[4096];
        for (;;)
        {
            const auto count = ::read(child.read, buffer, sizeof(buffer));
            if (count == 0)
                break;
            if (count < 0)
            {
                if (errno == EINTR)
                    continue;
                return detail::dialogError("Cannot read dialog result");
            }
            if (output.size() + std::size_t(count) > 65536)
                return detail::dialogError("Dialog output exceeded limit");
            output.append(buffer, std::size_t(count));
        }
        close(child.read);
        child.read = -1;
        int status = 0;
        pid_t waited;
        do
        {
            waited = waitpid(child.pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited < 0)
        {
            child.pid = -1;
            return detail::dialogError("Cannot collect dialog process result");
        }
        child.pid = -1;
        if (!WIFEXITED(status))
            return detail::dialogError("Dialog process terminated unexpectedly");
        if (WEXITSTATUS(status) == 1)
            return {{ServiceStatus::Cancelled, {}}, {}};
        if (WEXITSTATUS(status) != 0)
            return detail::dialogError("Zenity failed (exit " + std::to_string(WEXITSTATUS(status)) + ")");
        if (!output.empty() && output.back() == '\n')
            output.pop_back(); // Remove only the protocol terminator, not filename whitespace.
        return detail::normalizedDialogResult({{ServiceStatus::Success, {}}, std::move(output)});
    }
};
#else
class NativeBackend final : public FileDialogBackend
{
public:
    explicit NativeBackend(NativeDialogOptions) {}
    FileDialogCapabilities capabilities() const override
    {
        return {};
    }
    FileDialogResult show(const FileDialogOptions&) override
    {
        return {{ServiceStatus::Unavailable, "No native file dialog backend for this platform"}, {}};
    }
};
#endif
} // namespace
std::shared_ptr<FileDialogBackend> createNativeFileDialogs(NativeDialogOptions options)
{
    return std::make_shared<NativeBackend>(std::move(options));
}
} // namespace vectorgl::ui
