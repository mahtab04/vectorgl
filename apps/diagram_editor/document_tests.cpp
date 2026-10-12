#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "document.hpp"

namespace
{
void expect(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename F> void rejects(F function)
{
    bool threw = false;
    try
    {
        function();
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    expect(threw, "Malformed document was accepted");
}
} // namespace
int main()
{
    try
    {
        auto d = flow::starterDocument();
        d.name = "Quoted \"board\" / Caf\xc3\xa9";
        d.cards.front().description = "Line one\nLine two\t\\ <&>";
        expect(flow::parse(flow::serialize(d)) == d, "UTF-8 JSON roundtrip failed");
        auto json = flow::serialize(d);
        auto escaped = json;
        const auto at = escaped.find("Caf\xc3\xa9");
        escaped.replace(at, 5, "Caf\\u00e9");
        expect(flow::parse(escaped).name == d.name, "Unicode escape decode failed");
        rejects([&] { flow::parse(json + "garbage"); });
        rejects([&] { flow::parse("{\"version\":1,\"version\":1}"); });
        rejects(
            [&]
            {
                auto bad = d;
                bad.cards[1].id = bad.cards[0].id;
                flow::parse(flow::serialize(bad));
            });
        rejects(
            [&]
            {
                auto bad = d;
                bad.connections[0].to = 999;
                flow::parse(flow::serialize(bad));
            });
        rejects(
            [&]
            {
                auto bad = d;
                bad.connections[0].to = bad.connections[0].from;
                flow::parse(flow::serialize(bad));
            });
        rejects(
            [&]
            {
                auto bad = d;
                bad.connections.push_back({bad.nextId++, 1, 2});
                flow::parse(flow::serialize(bad));
            });
        rejects(
            [&]
            {
                auto bad = d;
                bad.cards[0].x = 1000001;
                flow::parse(flow::serialize(bad));
            });
        rejects(
            [&]
            {
                auto bad = d;
                bad.cards[0].title = std::string(1025, 'a');
                flow::parse(flow::serialize(bad));
            });
        rejects([&] { flow::parse(std::string(2 * 1024 * 1024 + 1, ' ')); });
        rejects([&] { flow::parse("{\"version\":1e999}"); });
        rejects([&] { flow::parse("{\"version\":01}"); });
        rejects([&] { flow::parse("{\"version\":1,\"name\":\"\\ud800\"}"); });
        flow::Editor editor;
        const auto initial = editor.document;
        const auto id = editor.add(0, 12, 30);
        expect(editor.connect(1, id), "Connection creation failed");
        expect(!editor.connect(1, id) && !editor.connect(id, id), "Duplicate/self connection accepted");
        const auto connected = editor.document;
        editor.erase({id});
        expect(!editor.document.find(id) && editor.document.connections.size() == initial.connections.size(),
               "Delete left dangling edges");
        expect(editor.undo() && editor.document == connected, "Undo failed");
        expect(editor.redo() && !editor.document.find(id), "Redo failed");
        editor.undo();
        const auto copies = editor.duplicate({1, id});
        expect(copies.size() == 2 && editor.document.cards.size() == connected.cards.size() + 2 &&
                   editor.document.connections.size() == connected.connections.size() + 1,
               "Duplicate did not remap internal connections");
        const auto beforeFailure = editor.document;
        rejects(
            [&]
            {
                editor.change(
                    [](flow::Document& doc)
                    {
                        doc.cards.clear();
                        throw std::runtime_error("rollback");
                    });
            });
        expect(editor.document == beforeFailure, "Failed edit was not rolled back");
        editor.undo();
        editor.add(2, 0, 0);
        expect(!editor.canRedo(), "New edits retained stale redo");
        editor.replace(initial);
        expect(!editor.canUndo() && !editor.canRedo(), "Open did not reset history");
        for (int i = 0; i < 110; ++i)
            editor.change([&](flow::Document& doc) { doc.name = std::to_string(i); });
        int count = 0;
        while (editor.undo())
            ++count;
        expect(count == 100, "Undo history limit incorrect");
        const auto svg = flow::exportSvg(d);
        expect(svg.find("&lt;&amp;&gt;") != std::string::npos && svg.find("<path") != std::string::npos,
               "SVG export did not escape text or include connections");
        const auto directory =
            std::filesystem::temp_directory_path() /
            ("flow-model-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("Test directory already exists");
        try
        {
            const auto file = directory / "board.json";
            flow::writeAtomic(file, flow::serialize(d));
            expect(flow::readDocument(file) == d, "Saved document did not reopen");
            d.name = "Replacement";
            flow::writeAtomic(file, flow::serialize(d));
            expect(flow::readDocument(file) == d, "Atomic replacement failed");
            rejects([&] { flow::writeAtomic(directory / "missing" / "board.json", "bad"); });
            expect(flow::readDocument(file) == d, "Failed save changed original document");
            for (const auto& entry : std::filesystem::directory_iterator(directory))
                expect(entry.path() == file, "Temporary save file leaked");
        }
        catch (...)
        {
            std::filesystem::remove_all(directory);
            throw;
        }
        std::filesystem::remove_all(directory);
        std::cout << "Flow Studio document, history, validation and file tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
