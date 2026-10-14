#pragma once
#include <memory>
#include <optional>

#include "document.hpp"

namespace flow
{
struct ViewState
{
    float zoom = 1, x = 0, y = 0;
    bool operator==(const ViewState&) const = default;
};
struct WorkspaceSettings
{
    bool dark = false, grid = true, snap = true;
    ViewState view;
    std::filesystem::path source;
    std::vector<std::filesystem::path> recent;
    bool operator==(const WorkspaceSettings&) const = default;
};
struct Recovery
{
    Document document, baseline;
    std::filesystem::path source;
    ViewState view;
    uint64_t timestamp = 0;
    bool operator==(const Recovery&) const = default;
};
std::string serializeSettings(const WorkspaceSettings& settings);
WorkspaceSettings parseSettings(const std::string& content);
std::string serializeRecovery(const Recovery& recovery);
Recovery parseRecovery(const std::string& content);

/*! One app instance per workspace directory. Locks are released by the OS on
 * crash. Recovery is never written to the source board. Pending startup recovery
 * (including unreadable recovery) is protected until explicitly resolved.
 */
class Workspace
{
public:
    explicit Workspace(std::filesystem::path directory);
    ~Workspace();
    Workspace(const Workspace&) = delete;
    Workspace& operator=(const Workspace&) = delete;
    static std::filesystem::path defaultDirectory();
    WorkspaceSettings settings;
    std::optional<Recovery> recovery;
    std::string warning, recoveryError;
    bool pendingRecovery() const
    {
        return pending_;
    }
    const std::filesystem::path& directory() const
    {
        return directory_;
    }
    void remember(const std::filesystem::path& path);
    void checkDocumentPath(const std::filesystem::path& path) const;
    void persistSettings();
    // Returns false when unresolved startup recovery prevents replacement.
    bool checkpoint(const Recovery& recovery);
    void discardRecovery();
    void acceptRecovery()
    {
        if (recovery)
            pending_ = false;
    }
    void clearOwnedRecovery();

private:
    struct Lock;
    std::filesystem::path directory_;
    std::unique_ptr<Lock> lock_;
    bool pending_ = false;
};
} // namespace flow
