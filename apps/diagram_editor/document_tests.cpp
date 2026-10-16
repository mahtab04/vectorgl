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
        editor.replace(initial);
        const auto clipboard = flow::copyCards(initial, {1, 2, 2});
        expect(!clipboard.empty() && flow::copyCards(initial, {}).empty(),
               "Empty or duplicate selection handling failed");
        const auto pasted = editor.paste(clipboard, 100, 200);
        expect(pasted.size() == 2 && pasted[0] != 1 && pasted[1] != 2, "Paste did not allocate fresh IDs");
        expect(editor.document.find(pasted[0])->x == 100 && editor.document.find(pasted[1])->x == 400,
               "Paste did not preserve relative positions");
        expect(editor.document.connections.size() == initial.connections.size() + 1 &&
                   editor.document.connections.back().from == pasted[0] &&
                   editor.document.connections.back().to == pasted[1],
               "Paste included external edges or failed to remap endpoints");
        const auto afterPaste = editor.document;
        expect(editor.undo() && editor.document == initial && editor.redo() && editor.document == afterPaste,
               "Paste was not one reversible transaction");
        rejects([&] { editor.paste("ordinary clipboard text", 0, 0); });
        rejects([&] { editor.paste("FLOWSTUDIO-CARDS/1\n{}", 0, 0); });
        rejects([&] { editor.paste(std::string(2 * 1024 * 1024 + 33, 'x'), 0, 0); });
        expect(editor.document == afterPaste, "Invalid clipboard mutated the document");
        editor.replace(initial);
        expect(editor.arrange({1, 2}, flow::Arrangement::Left) && editor.document.find(2)->x == 0,
               "Left alignment failed");
        expect(editor.undo() && editor.document == initial, "Alignment undo failed");
        editor.arrange({1, 2}, flow::Arrangement::CenterX);
        expect(editor.document.find(1)->x == 150 && editor.document.find(2)->x == 150, "Center alignment failed");
        editor.replace(initial);
        editor.document.find(2)->x = 100;
        const auto beforeDistribution = editor.document;
        editor.arrange({1, 2, 3}, flow::Arrangement::DistributeX);
        expect(editor.document.find(2)->x == 300 && editor.document.find(1)->x == 0 &&
                   editor.document.find(3)->x == 600,
               "Distribution did not preserve extremes and equalize spacing");
        expect(editor.undo() && editor.document == beforeDistribution, "Distribution undo failed");
        auto arrangementBase = initial;
        arrangementBase.find(2)->y = 40;
        arrangementBase.find(3)->y = 220;
        const std::pair<flow::Arrangement, float> arrangements[] = {{flow::Arrangement::Right, 600.0f},
                                                                    {flow::Arrangement::Top, 0.0f},
                                                                    {flow::Arrangement::CenterY, 110.0f},
                                                                    {flow::Arrangement::Bottom, 220.0f}};
        for (const auto& [mode, expected] : arrangements)
        {
            editor.replace(arrangementBase);
            editor.arrange({1, 2, 3}, mode);
            for (auto selected : {1, 2, 3})
                expect((mode == flow::Arrangement::Right ? editor.document.find(selected)->x
                                                         : editor.document.find(selected)->y) == expected,
                       "Arrangement mode produced incorrect bounds");
            expect(*editor.document.find(4) == *arrangementBase.find(4), "Arrangement changed an unselected card");
            expect(editor.undo() && editor.document == arrangementBase, "Arrangement was not one undo transaction");
        }
        editor.replace(arrangementBase);
        editor.arrange({1, 2, 3}, flow::Arrangement::DistributeY);
        expect(editor.document.find(2)->y == 110 && editor.document.find(1)->y == 0 &&
                   editor.document.find(3)->y == 220,
               "Vertical distribution changed extremes or spacing");
        auto unicode = initial;
        unicode.find(1)->title = "Caf\xc3\xa9";
        unicode.find(1)->kind = 2;
        unicode.find(1)->color = 5;
        editor.replace(initial);
        const auto boundaryPaste = editor.paste(flow::copyCards(unicode, {1, 2}), 999999, 999999);
        expect(editor.document.find(boundaryPaste[0])->title == unicode.find(1)->title &&
                   editor.document.find(boundaryPaste[0])->kind == 2 &&
                   editor.document.find(boundaryPaste[0])->color == 5,
               "Clipboard lost Unicode text, card type or style");
        expect(editor.document.find(boundaryPaste[1])->x == 1000000 &&
                   editor.document.find(boundaryPaste[1])->x - editor.document.find(boundaryPaste[0])->x == 300,
               "Boundary paste distorted group placement");
        editor.replace(initial);
        expect(!editor.arrange({1}, flow::Arrangement::Top) && !editor.canUndo(), "No-op alignment recorded history");
        editor.move({1, 2}, 10, -5);
        expect(editor.document.find(1)->x == 10 && editor.document.find(2)->x == 310 &&
                   editor.document.find(1)->y == -5,
               "Selection movement changed relative placement");
        expect(editor.undo() && editor.document == initial, "Movement undo failed");
        editor.document.find(1)->x = 999998;
        editor.document.find(2)->x = 999999;
        editor.move({1, 2}, 10, 0);
        expect(editor.document.find(1)->x == 999999 && editor.document.find(2)->x == 1000000,
               "Boundary movement distorted group spacing");
        editor.replace(initial);
        while (editor.document.cards.size() < flow::maxCards)
        {
            auto card = initial.cards.front();
            card.id = editor.document.nextId++;
            editor.document.cards.push_back(card);
        }
        const auto full = editor.document;
        rejects([&] { editor.paste(clipboard, 0, 0); });
        expect(editor.document == full && !editor.canUndo(), "Over-limit paste partially mutated the document");
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
