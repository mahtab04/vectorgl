#include "workspace.hpp"

#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <fstream>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace flow
{
namespace
{
std::string readBounded(const std::filesystem::path& path, std::size_t limit)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 0 || file.tellg() > static_cast<std::streamoff>(limit))
        throw std::runtime_error("Workspace file cannot be read or exceeds its size limit");
    std::string result(static_cast<std::size_t>(file.tellg()), '\0');
    file.seekg(0);
    if (!file.read(result.data(), static_cast<std::streamsize>(result.size())))
        throw std::runtime_error("Workspace file could not be read completely");
    return result;
}
} // namespace
struct Workspace::Lock
{
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    explicit Lock(const std::filesystem::path& path)
    {
        handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                             nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Workspace is unavailable or already open in another Flow Studio instance. Use "
                                     "--state-dir for a separate workspace.");
    }
    ~Lock()
    {
        CloseHandle(handle);
    }
#else
    int handle = -1;
    explicit Lock(const std::filesystem::path& path)
    {
        handle = ::open(path.c_str(), O_CREAT | O_RDWR, 0600);
        if (handle < 0 || flock(handle, LOCK_EX | LOCK_NB) != 0)
        {
            if (handle >= 0)
                ::close(handle);
            throw std::runtime_error("Workspace is unavailable or already open in another Flow Studio instance. Use "
                                     "--state-dir for a separate workspace.");
        }
    }
    ~Lock()
    {
        ::close(handle);
    }
#endif
};
std::filesystem::path Workspace::defaultDirectory()
{
#ifdef _WIN32
    const auto count = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (count)
    {
        std::wstring path(count, L'\0');
        const auto written = GetEnvironmentVariableW(L"LOCALAPPDATA", path.data(), count);
        if (written && written < count)
        {
            path.resize(written);
            return std::filesystem::path(path) / L"FlowStudio";
        }
    }
#else
#ifndef __APPLE__
    if (const char* state = std::getenv("XDG_STATE_HOME"))
        if (std::filesystem::path(state).is_absolute())
            return std::filesystem::path(state) / "flow-studio";
#endif
    if (const char* home = std::getenv("HOME"))
    {
#ifdef __APPLE__
        return std::filesystem::path(home) / "Library" / "Application Support" / "FlowStudio";
#else
        return std::filesystem::path(home) / ".local" / "state" / "flow-studio";
#endif
    }
#endif
    throw std::runtime_error("No workspace data directory found. Pass --state-dir path/to/folder.");
}
Workspace::Workspace(std::filesystem::path directory)
    : directory_(std::filesystem::absolute(directory).lexically_normal())
{
    std::filesystem::create_directories(directory_);
    lock_ = std::make_unique<Lock>(directory_ / "workspace.lock");
    if (std::filesystem::exists(directory_ / "settings.json"))
        try
        {
            settings = parseSettings(readBounded(directory_ / "settings.json", 128 * 1024));
        }
        catch (const std::exception& error)
        {
            warning = std::string("Settings could not be loaded: ") + error.what();
        }
    pending_ = std::filesystem::exists(directory_ / "recovery.json");
    if (pending_)
        try
        {
            recovery = parseRecovery(readBounded(directory_ / "recovery.json", 12 * 1024 * 1024));
        }
        catch (const std::exception& error)
        {
            recoveryError = error.what();
        }
}
Workspace::~Workspace() = default;
void Workspace::checkDocumentPath(const std::filesystem::path& path) const
{
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    for (const char* name : {"settings.json", "recovery.json", "workspace.lock"})
    {
        const auto internal = directory_ / name;
        bool same = absolute == internal;
#ifdef _WIN32
        auto a = absolute.wstring(), b = internal.wstring();
        std::transform(a.begin(), a.end(), a.begin(), [](wchar_t c) { return std::towlower(c); });
        std::transform(b.begin(), b.end(), b.begin(), [](wchar_t c) { return std::towlower(c); });
        same = same || a == b;
#endif
        std::error_code error;
        if (same || std::filesystem::equivalent(absolute, internal, error))
            throw std::runtime_error("Choose a document outside the reserved workspace settings/recovery files");
    }
}
void Workspace::remember(const std::filesystem::path& path)
{
    if (path.empty())
        return;
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    std::erase_if(settings.recent,
                  [&](const auto& item)
                  {
                      std::error_code error;
                      return item == absolute || std::filesystem::equivalent(item, absolute, error);
                  });
    settings.recent.insert(settings.recent.begin(), absolute);
    if (settings.recent.size() > 10)
        settings.recent.resize(10);
}
void Workspace::persistSettings()
{
    const auto content = serializeSettings(settings);
    (void)parseSettings(content);
    writeAtomic(directory_ / "settings.json", content);
}
bool Workspace::checkpoint(const Recovery& value)
{
    if (pending_)
        return false;
    if (!value.source.empty())
        checkDocumentPath(value.source);
    if (value.document == value.baseline)
    {
        clearOwnedRecovery();
        return true;
    }
    const auto content = serializeRecovery(value);
    (void)parseRecovery(content);
    writeAtomic(directory_ / "recovery.json", content);
    recovery = value;
    return true;
}
void Workspace::discardRecovery()
{
    std::filesystem::remove(directory_ / "recovery.json");
    recovery.reset();
    recoveryError.clear();
    pending_ = false;
}
void Workspace::clearOwnedRecovery()
{
    if (!pending_)
        discardRecovery();
}
} // namespace flow
