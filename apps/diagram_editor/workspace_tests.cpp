#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "workspace.hpp"

namespace
{
void expect(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <typename F> void rejects(F function)
{
    bool rejected = false;
    try
    {
        function();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    expect(rejected, "Invalid workspace operation was accepted");
}
std::string contents(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
} // namespace
int main()
{
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("flow-workspace-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("Test directory already exists");
        try
        {
            const auto source = directory / std::filesystem::path(std::u8string(u8"Café board.json"));
            const auto original = flow::starterDocument();
            auto changed = original;
            changed.name = "Unsaved work";
            flow::writeAtomic(source, flow::serialize(original));
            const auto originalBytes = contents(source);
            const flow::Recovery snapshot{changed, original, source, {0.8f, 200, -90}, 1790000000};
            expect(flow::parseRecovery(flow::serializeRecovery(snapshot)) == snapshot, "Recovery roundtrip failed");
            auto invalid = snapshot;
            invalid.view.zoom = 0;
            rejects([&] { flow::parseRecovery(flow::serializeRecovery(invalid)); });
            invalid = snapshot;
            invalid.document.nextId = 0;
            rejects([&] { flow::parseRecovery(flow::serializeRecovery(invalid)); });
            rejects([&] { flow::parseRecovery(std::string(12 * 1024 * 1024 + 1, ' ')); });
            rejects([&] { flow::parseSettings("{}"); });
            flow::WorkspaceSettings settings;
            settings.dark = true;
            settings.grid = false;
            settings.snap = false;
            settings.source = source;
            settings.view = snapshot.view;
            settings.recent = {source};
            expect(flow::parseSettings(flow::serializeSettings(settings)) == settings, "Preferences roundtrip failed");
            auto badSettings = settings;
            badSettings.view.x = std::numeric_limits<float>::infinity();
            rejects([&] { flow::parseSettings(flow::serializeSettings(badSettings)); });
            badSettings = settings;
            badSettings.recent.push_back(source);
            rejects([&] { flow::parseSettings(flow::serializeSettings(badSettings)); });
            badSettings = settings;
            badSettings.source = "relative.json";
            rejects([&] { flow::parseSettings(flow::serializeSettings(badSettings)); });
            const auto profile = directory / "profile";
            {
                flow::Workspace workspace(profile);
                rejects([&] { workspace.checkDocumentPath(profile / "recovery.json"); });
                rejects([&] { workspace.checkDocumentPath(profile / "settings.json"); });
#ifdef _WIN32
                rejects([&] { workspace.checkDocumentPath(profile / "RECOVERY.JSON"); });
#endif
                workspace.settings = settings;
                workspace.remember(source);
                workspace.remember(source.parent_path() / "." / source.filename());
                expect(workspace.settings.recent.size() == 1, "Recent file deduplication failed");
                for (int i = 0; i < 14; ++i)
                    workspace.remember(directory / (std::to_string(i) + ".json"));
                expect(workspace.settings.recent.size() == 10 &&
                           workspace.settings.recent.front().filename() == "13.json",
                       "Recent files not bounded or ordered by use");
                workspace.remember(source);
                workspace.persistSettings();
                expect(workspace.checkpoint(snapshot), "Dirty recovery checkpoint failed");
                expect(contents(source) == originalBytes, "Recovery overwrote the source board");
                rejects([&] { flow::Workspace other(profile); });
            }
            {
                flow::Workspace reopened(profile);
                expect(reopened.pendingRecovery() && reopened.recovery == snapshot,
                       "Interrupted session did not offer recovery");
                expect(reopened.settings.dark && !reopened.settings.grid && !reopened.settings.snap &&
                           reopened.settings.view == settings.view && reopened.settings.recent.front() == source,
                       "Preferences or recent files did not survive restart");
                const auto before = contents(profile / "recovery.json");
                auto different = snapshot;
                different.document.name = "Different session";
                expect(!reopened.checkpoint(different), "Pending recovery was overwritten");
                reopened.clearOwnedRecovery();
                expect(contents(profile / "recovery.json") == before, "Unresolved recovery was deleted");
                reopened.acceptRecovery();
                expect(reopened.checkpoint(different), "Accepted recovery could not be updated");
                auto clean = different;
                clean.baseline = clean.document;
                reopened.checkpoint(clean);
                expect(!std::filesystem::exists(profile / "recovery.json"), "Clean/saved session left stale recovery");
                auto untitled = snapshot;
                untitled.source.clear();
                reopened.checkpoint(untitled);
            }
            {
                flow::Workspace reopened(profile);
                expect(reopened.pendingRecovery() && reopened.recovery->source.empty(),
                       "Untitled board was not recoverable");
                reopened.discardRecovery();
                expect(!reopened.pendingRecovery() && !std::filesystem::exists(profile / "recovery.json"),
                       "Explicit discard failed");
                // A failed replacement must retain the previous snapshot.
                reopened.checkpoint(snapshot);
                auto oversized = snapshot;
                oversized.document.cards.front().title = std::string(1025, 'a');
                rejects([&] { reopened.checkpoint(oversized); });
                expect(flow::parseRecovery(contents(profile / "recovery.json")) == snapshot,
                       "Failed checkpoint lost prior recovery");
            }
            flow::writeAtomic(profile / "recovery.json", "broken recovery");
            flow::writeAtomic(profile / "settings.json", "broken settings");
            {
                flow::Workspace corrupt(profile);
                expect(corrupt.pendingRecovery() && !corrupt.recovery && !corrupt.recoveryError.empty() &&
                           !corrupt.warning.empty(),
                       "Corrupt workspace data was not reported safely");
                expect(!corrupt.checkpoint(snapshot) && contents(profile / "recovery.json") == "broken recovery",
                       "Corrupt recovery was silently overwritten");
                corrupt.discardRecovery();
                expect(corrupt.checkpoint(snapshot), "Explicit corrupt recovery discard did not unblock snapshots");
            }
            expect(contents(source) == originalBytes, "Workspace operations changed a saved document");
        }
        catch (...)
        {
            std::filesystem::remove_all(directory);
            throw;
        }
        std::filesystem::remove_all(directory);
        std::cout << "Workspace preferences, recovery, locking and persistence tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
