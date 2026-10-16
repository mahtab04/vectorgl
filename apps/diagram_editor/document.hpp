#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace flow
{
struct Card
{
    uint32_t id = 0;
    float x = 0, y = 0;
    std::string title, description;
    int kind = 0, color = 0;
    bool operator==(const Card&) const = default;
};
struct Connection
{
    uint32_t id = 0, from = 0, to = 0;
    bool operator==(const Connection&) const = default;
};
struct Document
{
    std::string name = "Untitled board";
    std::vector<Card> cards;
    std::vector<Connection> connections;
    uint32_t nextId = 1;
    bool operator==(const Document&) const = default;
    Card* find(uint32_t id);
    const Card* find(uint32_t id) const;
};
constexpr float cardWidth = 224, cardHeight = 132;
constexpr std::size_t maxCards = 2000, maxConnections = 4000;
Document starterDocument();
std::string serialize(const Document& document);
Document parse(const std::string& json);
Document readDocument(const std::filesystem::path& path);
void writeAtomic(const std::filesystem::path& path, const std::string& content);
std::string exportSvg(const Document& document);
enum class Arrangement
{
    Left,
    CenterX,
    Right,
    Top,
    CenterY,
    Bottom,
    DistributeX,
    DistributeY
};
std::string copyCards(const Document& document, const std::vector<uint32_t>& ids);

class Editor
{
public:
    Document document = starterDocument();
    bool change(const std::function<void(Document&)>& action);
    void record(const Document& before);
    bool undo();
    bool redo();
    bool canUndo() const
    {
        return !undo_.empty();
    }
    bool canRedo() const
    {
        return !redo_.empty();
    }
    void replace(Document value);
    uint32_t add(int kind, float x, float y);
    bool connect(uint32_t from, uint32_t to);
    void erase(const std::vector<uint32_t>& ids, uint32_t connection = 0);
    std::vector<uint32_t> duplicate(const std::vector<uint32_t>& ids);
    std::vector<uint32_t> paste(const std::string& payload, float x, float y);
    bool arrange(const std::vector<uint32_t>& ids, Arrangement arrangement);
    bool move(const std::vector<uint32_t>& ids, float dx, float dy, bool recordHistory = true);

private:
    std::vector<Document> undo_, redo_;
    static void trim(std::vector<Document>& stack);
};
} // namespace flow
