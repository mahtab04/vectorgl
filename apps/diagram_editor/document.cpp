#include "document.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "workspace.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace flow
{
namespace
{
constexpr std::size_t maxFile = 2 * 1024 * 1024;
struct Json
{
    enum Kind
    {
        Object,
        Array,
        String,
        Number,
        Other
    } kind = Other;
    std::map<std::string, Json> object;
    std::vector<Json> array;
    std::string string;
    double number = 0;
    const Json& at(const std::string& key) const
    {
        if (kind != Object || !object.count(key))
            throw std::runtime_error("Missing document field: " + key);
        return object.at(key);
    }
    std::string text(std::size_t limit) const
    {
        if (kind != String || string.size() > limit)
            throw std::runtime_error("Invalid or oversized document text");
        return string;
    }
    double numeric(double minimum, double maximum) const
    {
        if (kind != Number || !std::isfinite(number) || number < minimum || number > maximum)
            throw std::runtime_error("Invalid document number");
        return number;
    }
    uint32_t integer(uint32_t minimum, uint32_t maximum) const
    {
        const auto value = numeric(minimum, maximum);
        if (value != std::floor(value))
            throw std::runtime_error("Document ID must be an integer");
        return static_cast<uint32_t>(value);
    }
};
void appendUtf8(std::string& output, uint32_t cp)
{
    if (cp <= 0x7F)
        output += static_cast<char>(cp);
    else
    {
        if (cp > 0xFFFF)
            output += static_cast<char>(0xF0 | (cp >> 18));
        if (cp > 0x7FF)
            output += static_cast<char>((cp > 0xFFFF ? 0x80 : 0xE0) | ((cp >> 12) & 0x3F));
        output += static_cast<char>((cp > 0x7FF ? 0x80 : 0xC0) | ((cp >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (cp & 0x3F));
    }
}
bool validUtf8(const std::string& value)
{
    for (std::size_t i = 0; i < value.size();)
    {
        const auto byte = static_cast<unsigned char>(value[i++]);
        if (byte < 128)
            continue;
        int count;
        uint32_t cp, minimum;
        if (byte >= 0xC2 && byte <= 0xDF)
        {
            count = 1;
            cp = byte & 31;
            minimum = 128;
        }
        else if (byte >= 0xE0 && byte <= 0xEF)
        {
            count = 2;
            cp = byte & 15;
            minimum = 2048;
        }
        else if (byte >= 0xF0 && byte <= 0xF4)
        {
            count = 3;
            cp = byte & 7;
            minimum = 65536;
        }
        else
            return false;
        while (count--)
        {
            if (i == value.size())
                return false;
            const auto next = static_cast<unsigned char>(value[i++]);
            if ((next & 0xC0) != 0x80)
                return false;
            cp = (cp << 6) | (next & 63);
        }
        if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
    }
    return true;
}
class Parser
{
    const std::string& input;
    std::size_t position = 0;
    std::size_t values = 0;
    [[noreturn]] void error() const
    {
        throw std::runtime_error("Invalid JSON at byte " + std::to_string(position));
    }
    void space()
    {
        while (position < input.size() && (input[position] == ' ' || input[position] == '\n' ||
                                           input[position] == '\r' || input[position] == '\t'))
            ++position;
    }
    bool take(char c)
    {
        space();
        if (position < input.size() && input[position] == c)
        {
            ++position;
            return true;
        }
        return false;
    }
    uint32_t hex()
    {
        uint32_t cp = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (position == input.size())
                error();
            const char c = input[position++];
            const int digit = c >= '0' && c <= '9'   ? c - '0'
                              : c >= 'a' && c <= 'f' ? c - 'a' + 10
                              : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                     : -1;
            if (digit < 0)
                error();
            cp = cp * 16 + digit;
        }
        return cp;
    }
    std::string string()
    {
        if (!take('"'))
            error();
        std::string result;
        while (position < input.size())
        {
            const char c = input[position++];
            if (c == '"')
                return result;
            if (static_cast<unsigned char>(c) < 32)
                error();
            if (c != '\\')
            {
                result += c;
                continue;
            }
            if (position == input.size())
                error();
            const char escaped = input[position++];
            switch (escaped)
            {
            case '"':
            case '\\':
            case '/':
                result += escaped;
                break;
            case 'b':
                result += '\b';
                break;
            case 'f':
                result += '\f';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            case 'u':
            {
                auto cp = hex();
                if (cp >= 0xD800 && cp <= 0xDBFF)
                {
                    if (position + 2 > input.size() || input.substr(position, 2) != "\\u")
                        error();
                    position += 2;
                    const auto low = hex();
                    if (low < 0xDC00 || low > 0xDFFF)
                        error();
                    cp = 0x10000 + ((cp - 0xD800) << 10) + low - 0xDC00;
                }
                else if (cp >= 0xDC00 && cp <= 0xDFFF)
                    error();
                appendUtf8(result, cp);
                break;
            }
            default:
                error();
            }
        }
        error();
    }
    Json value(int depth)
    {
        if (depth > 16 || ++values > 100000)
            error();
        Json result;
        if (take('{'))
        {
            result.kind = Json::Object;
            if (take('}'))
                return result;
            do
            {
                auto key = string();
                if (!take(':'))
                    error();
                if (!result.object.emplace(key, value(depth + 1)).second)
                    error();
            } while (take(','));
            if (!take('}'))
                error();
        }
        else if (take('['))
        {
            result.kind = Json::Array;
            if (take(']'))
                return result;
            do
            {
                result.array.push_back(value(depth + 1));
            } while (take(','));
            if (!take(']'))
                error();
        }
        else
        {
            space();
            if (position == input.size())
                error();
            if (input[position] == '"')
            {
                result.kind = Json::String;
                result.string = string();
                return result;
            }
            for (const char* literal : {"true", "false", "null"})
                if (input.compare(position, std::char_traits<char>::length(literal), literal) == 0)
                {
                    position += std::char_traits<char>::length(literal);
                    return result;
                }
            const auto begin = position;
            if (input[position] == '-')
                ++position;
            if (position == input.size())
                error();
            if (input[position] == '0')
                ++position;
            else
            {
                if (input[position] < '1' || input[position] > '9')
                    error();
                while (position < input.size() && input[position] >= '0' && input[position] <= '9')
                    ++position;
            }
            if (position < input.size() && input[position] == '.')
            {
                ++position;
                const auto digits = position;
                while (position < input.size() && input[position] >= '0' && input[position] <= '9')
                    ++position;
                if (position == digits)
                    error();
            }
            if (position < input.size() && (input[position] == 'e' || input[position] == 'E'))
            {
                ++position;
                if (position < input.size() && (input[position] == '+' || input[position] == '-'))
                    ++position;
                const auto digits = position;
                while (position < input.size() && input[position] >= '0' && input[position] <= '9')
                    ++position;
                if (position == digits)
                    error();
            }
            result.kind = Json::Number;
            if (std::from_chars(input.data() + begin, input.data() + position, result.number).ec != std::errc{} ||
                !std::isfinite(result.number))
                error();
        }
        return result;
    }

public:
    explicit Parser(const std::string& source) : input(source) {}
    Json run()
    {
        auto result = value(0);
        space();
        if (position != input.size())
            error();
        return result;
    }
};
std::string quote(const std::string& value)
{
    std::string output = "\"";
    const char* digits = "0123456789abcdef";
    for (unsigned char c : value)
    {
        if (c == '"' || c == '\\')
        {
            output += '\\';
            output += static_cast<char>(c);
        }
        else if (c < 32)
        {
            output += "\\u00";
            output += digits[c >> 4];
            output += digits[c & 15];
        }
        else
            output += static_cast<char>(c);
    }
    return output + '"';
}
std::size_t memory(const Document& d)
{
    std::size_t bytes = sizeof(d) + d.name.capacity() + d.cards.capacity() * sizeof(Card) +
                        d.connections.capacity() * sizeof(Connection);
    for (const auto& card : d.cards)
        bytes += card.title.capacity() + card.description.capacity();
    return bytes;
}
std::string xml(const std::string& value)
{
    std::string output;
    for (unsigned char c : value)
        if (c == '&')
            output += "&amp;";
        else if (c == '<')
            output += "&lt;";
        else if (c == '>')
            output += "&gt;";
        else if (c == '"')
            output += "&quot;";
        else if (c >= 32 || c == '\n' || c == '\t')
            output += static_cast<char>(c);
    return output;
}
} // namespace

Card* Document::find(uint32_t id)
{
    for (auto& card : cards)
        if (card.id == id)
            return &card;
    return nullptr;
}
const Card* Document::find(uint32_t id) const
{
    for (const auto& card : cards)
        if (card.id == id)
            return &card;
    return nullptr;
}
Document starterDocument()
{
    Document d;
    d.name = "Product launch";
    d.cards = {{1, 0, 0, "Discover", "Understand the problem and your audience", 0, 0},
               {2, 300, 0, "Design", "Turn ideas into a thoughtful experience", 0, 1},
               {3, 600, 0, "Review", "Is the experience ready to build?", 1, 2},
               {4, 600, 220, "Build", "Bring the design to life", 0, 3},
               {5, 300, 220, "Launch", "Share it with the world", 0, 4},
               {6, 0, 220, "Team notes", "Drag a port to connect. Make this board yours.", 2, 5}};
    d.connections = {{7, 1, 2}, {8, 2, 3}, {9, 3, 4}, {10, 4, 5}};
    d.nextId = 11;
    return d;
}
std::string serialize(const Document& d)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "{\n  \"version\": 1,\n  \"name\": " << quote(d.name) << ",\n  \"nextId\": " << d.nextId
        << ",\n  \"cards\": [";
    for (std::size_t i = 0; i < d.cards.size(); ++i)
    {
        const auto& c = d.cards[i];
        out << (i ? ",\n" : "\n") << "    {\"id\":" << c.id << ",\"x\":" << c.x << ",\"y\":" << c.y
            << ",\"title\":" << quote(c.title) << ",\"description\":" << quote(c.description) << ",\"kind\":" << c.kind
            << ",\"color\":" << c.color << '}';
    }
    out << "\n  ],\n  \"connections\": [";
    for (std::size_t i = 0; i < d.connections.size(); ++i)
    {
        const auto& e = d.connections[i];
        out << (i ? ",\n" : "\n") << "    {\"id\":" << e.id << ",\"from\":" << e.from << ",\"to\":" << e.to << '}';
    }
    out << "\n  ]\n}\n";
    return out.str();
}
Document parse(const std::string& source)
{
    if (source.size() > maxFile || !validUtf8(source))
        throw std::runtime_error("Document exceeds 2 MiB or contains invalid UTF-8");
    const auto root = Parser(source).run();
    if (root.at("version").integer(1, 1) != 1)
        throw std::runtime_error("Unsupported document version");
    Document d;
    d.name = root.at("name").text(256);
    d.nextId = root.at("nextId").integer(1, 1000000000);
    const auto& cards = root.at("cards");
    const auto& edges = root.at("connections");
    if (cards.kind != Json::Array || cards.array.size() > maxCards || edges.kind != Json::Array ||
        edges.array.size() > maxConnections)
        throw std::runtime_error("Document contains too many cards or connections");
    std::set<uint32_t> ids;
    for (const auto& item : cards.array)
    {
        Card c;
        c.id = item.at("id").integer(1, d.nextId - 1);
        c.x = static_cast<float>(item.at("x").numeric(-1000000, 1000000));
        c.y = static_cast<float>(item.at("y").numeric(-1000000, 1000000));
        c.title = item.at("title").text(1024);
        c.description = item.at("description").text(4096);
        c.kind = item.at("kind").integer(0, 2);
        c.color = item.at("color").integer(0, 5);
        if (!ids.insert(c.id).second)
            throw std::runtime_error("Duplicate document ID");
        d.cards.push_back(std::move(c));
    }
    std::set<std::pair<uint32_t, uint32_t>> pairs;
    for (const auto& item : edges.array)
    {
        Connection e{item.at("id").integer(1, d.nextId - 1), item.at("from").integer(1, d.nextId - 1),
                     item.at("to").integer(1, d.nextId - 1)};
        if (!ids.insert(e.id).second || !d.find(e.from) || !d.find(e.to) || e.from == e.to ||
            !pairs.emplace(e.from, e.to).second)
            throw std::runtime_error("Invalid or duplicate connection");
        d.connections.push_back(e);
    }
    return d;
}
namespace
{
std::string pathString(const std::filesystem::path& path)
{
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
std::filesystem::path parsedPath(const Json& value)
{
    const auto text = value.text(4096);
    for (unsigned char c : text)
        if (c < 32)
            throw std::runtime_error("Workspace path contains control characters");
    auto path = std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
    if (!path.empty() && !path.is_absolute())
        throw std::runtime_error("Workspace paths must be absolute");
    return path.lexically_normal();
}
void writeView(std::ostream& out, const ViewState& view)
{
    out << "\"zoom\":" << view.zoom << ",\"panX\":" << view.x << ",\"panY\":" << view.y;
}
ViewState readView(const Json& root)
{
    return {static_cast<float>(root.at("zoom").numeric(0.15, 3)),
            static_cast<float>(root.at("panX").numeric(-1000000000, 1000000000)),
            static_cast<float>(root.at("panY").numeric(-1000000000, 1000000000))};
}
Json workspaceJson(const std::string& content, std::size_t limit)
{
    if (content.size() > limit || !validUtf8(content))
        throw std::runtime_error("Invalid or oversized workspace file");
    auto root = Parser(content).run();
    (void)root.at("version").integer(1, 1);
    return root;
}
} // namespace
std::string serializeSettings(const WorkspaceSettings& settings)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "{\"version\":1,\"dark\":" << settings.dark << ",\"grid\":" << settings.grid << ",\"snap\":" << settings.snap
        << ',';
    writeView(out, settings.view);
    out << ",\"source\":" << quote(pathString(settings.source)) << ",\"recent\":[";
    for (std::size_t i = 0; i < settings.recent.size(); ++i)
        out << (i ? "," : "") << quote(pathString(settings.recent[i]));
    return out.str() + "]}\n";
}
WorkspaceSettings parseSettings(const std::string& content)
{
    const auto root = workspaceJson(content, 128 * 1024);
    WorkspaceSettings result;
    result.dark = root.at("dark").integer(0, 1) != 0;
    result.grid = root.at("grid").integer(0, 1) != 0;
    result.snap = root.at("snap").integer(0, 1) != 0;
    result.view = readView(root);
    result.source = parsedPath(root.at("source"));
    const auto& list = root.at("recent");
    if (list.kind != Json::Array || list.array.size() > 10)
        throw std::runtime_error("Recent file list exceeds its limit");
    for (const auto& entry : list.array)
    {
        auto path = parsedPath(entry);
        if (path.empty() || std::find(result.recent.begin(), result.recent.end(), path) != result.recent.end())
            throw std::runtime_error("Invalid or duplicate recent file");
        result.recent.push_back(std::move(path));
    }
    return result;
}
std::string serializeRecovery(const Recovery& recovery)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "{\"version\":1,\"source\":" << quote(pathString(recovery.source)) << ",\"timestamp\":" << recovery.timestamp
        << ',';
    writeView(out, recovery.view);
    out << ",\"document\":" << quote(serialize(recovery.document))
        << ",\"baseline\":" << quote(serialize(recovery.baseline)) << "}\n";
    return out.str();
}
Recovery parseRecovery(const std::string& content)
{
    const auto root = workspaceJson(content, 12 * 1024 * 1024);
    Recovery result;
    result.source = parsedPath(root.at("source"));
    result.view = readView(root);
    const double timestamp = root.at("timestamp").numeric(0, 100000000000);
    if (timestamp != std::floor(timestamp))
        throw std::runtime_error("Recovery timestamp must be an integer");
    result.timestamp = static_cast<uint64_t>(timestamp);
    result.document = parse(root.at("document").text(2 * 1024 * 1024));
    result.baseline = parse(root.at("baseline").text(2 * 1024 * 1024));
    return result;
}
Document readDocument(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 0 || file.tellg() > static_cast<std::streamoff>(maxFile))
        throw std::runtime_error("Cannot open document or file exceeds 2 MiB");
    std::string content(static_cast<std::size_t>(file.tellg()), '\0');
    file.seekg(0);
    if (!file.read(content.data(), static_cast<std::streamsize>(content.size())))
        throw std::runtime_error("Could not read document");
    return parse(content);
}
void writeAtomic(const std::filesystem::path& path, const std::string& content)
{
    if (path.empty())
        throw std::runtime_error("Choose a file path first");
    auto temporary = path;
    temporary += ".flow-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp";
    if (std::filesystem::exists(temporary))
        throw std::runtime_error("Temporary file already exists");
    try
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        file.flush();
        if (!file)
            throw std::runtime_error("Could not write file; original document retained");
        file.close();
        if (!file)
            throw std::runtime_error("Could not close saved document");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace file; original document retained");
#else
        std::filesystem::rename(temporary, path);
#endif
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
std::string exportSvg(const Document& d)
{
    float left = 0, top = 0, right = 800, bottom = 500;
    if (!d.cards.empty())
    {
        left = right = d.cards.front().x;
        top = bottom = d.cards.front().y;
        for (const auto& c : d.cards)
        {
            left = std::min(left, c.x);
            top = std::min(top, c.y);
            right = std::max(right, c.x + cardWidth);
            bottom = std::max(bottom, c.y + cardHeight);
        }
    }
    for (const auto& e : d.connections)
        if (const auto* a = d.find(e.from))
            if (const auto* b = d.find(e.to))
            {
                const float x = a->x + cardWidth, tx = b->x;
                const float bend = std::max(70.0f, std::abs(tx - x) * 0.5f);
                left = std::min(left, tx - bend);
                right = std::max(right, x + bend);
            }
    const char* colors[] = {"#7661ed", "#329abb", "#e6983c", "#4d9c77", "#e06f8c", "#9a80c1"};
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << left - 40 << ' ' << top - 40 << ' '
        << right - left + 80 << ' ' << bottom - top + 80 << "\"><title>" << xml(d.name) << "</title><rect x=\""
        << left - 40 << "\" y=\"" << top - 40 << "\" width=\"100%\" height=\"100%\" fill=\"#f5f5fa\"/>";
    for (const auto& e : d.connections)
    {
        const auto* a = d.find(e.from);
        const auto* b = d.find(e.to);
        if (!a || !b)
            continue;
        const float x = a->x + cardWidth, y = a->y + cardHeight / 2, tx = b->x, ty = b->y + cardHeight / 2;
        const float bend = std::max(70.0f, std::abs(tx - x) * 0.5f);
        out << "<path d=\"M " << x << ' ' << y << " C " << x + bend << ' ' << y << ' ' << tx - bend << ' ' << ty << ' '
            << tx << ' ' << ty << "\" fill=\"none\" stroke=\"#9795b4\" stroke-width=\"2\"/>";
    }
    for (const auto& c : d.cards)
    {
        out << "<g><rect x=\"" << c.x << "\" y=\"" << c.y
            << "\" width=\"224\" height=\"132\" rx=\"14\" fill=\"white\" stroke=\"" << colors[c.color]
            << "\"/><rect x=\"" << c.x + 16 << "\" y=\"" << c.y + 18 << "\" width=\"8\" height=\"8\" rx=\"2\" fill=\""
            << colors[c.color] << "\"/><text x=\"" << c.x + 34 << "\" y=\"" << c.y + 28
            << "\" font-family=\"sans-serif\" font-size=\"16\" fill=\"#292638\">" << xml(c.title) << "</text>";
        // Wrap by UTF-8 scalar boundaries; exported SVG remains editable vector text.
        std::size_t start = 0;
        for (int line = 0; line < 3 && start < c.description.size(); ++line)
        {
            std::size_t end = start, count = 0, space = start;
            while (end < c.description.size() && count < 27)
            {
                if (c.description[end] == ' ')
                    space = end;
                ++end;
                while (end < c.description.size() && (static_cast<unsigned char>(c.description[end]) & 0xC0) == 0x80)
                    ++end;
                ++count;
            }
            if (end < c.description.size() && space > start)
                end = space;
            out << "<text x=\"" << c.x + 16 << "\" y=\"" << c.y + 62 + line * 19
                << "\" font-family=\"sans-serif\" font-size=\"13\" fill=\"#706b80\">"
                << xml(c.description.substr(start, end - start)) << "</text>";
            start = end;
            while (start < c.description.size() && c.description[start] == ' ')
                ++start;
        }
        out << "</g>";
    }
    return out.str() + "</svg>\n";
}
void Editor::trim(std::vector<Document>& stack)
{
    std::size_t bytes = 0;
    for (const auto& d : stack)
        bytes += memory(d);
    while (stack.size() > 100 || bytes > 16 * 1024 * 1024)
    {
        bytes -= memory(stack.front());
        stack.erase(stack.begin());
    }
}
void Editor::record(const Document& before)
{
    if (before == document)
        return;
    undo_.push_back(before);
    trim(undo_);
    redo_.clear();
}
bool Editor::change(const std::function<void(Document&)>& action)
{
    auto before = document;
    try
    {
        action(document);
    }
    catch (...)
    {
        document = std::move(before);
        throw;
    }
    const bool changed = before != document;
    record(before);
    return changed;
}
bool Editor::undo()
{
    if (undo_.empty())
        return false;
    redo_.push_back(document);
    trim(redo_);
    document = std::move(undo_.back());
    undo_.pop_back();
    return true;
}
bool Editor::redo()
{
    if (redo_.empty())
        return false;
    undo_.push_back(document);
    trim(undo_);
    document = std::move(redo_.back());
    redo_.pop_back();
    return true;
}
void Editor::replace(Document value)
{
    document = std::move(value);
    undo_.clear();
    redo_.clear();
}
uint32_t Editor::add(int kind, float x, float y)
{
    if (kind < 0 || kind > 2 || !std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 1000000 ||
        std::abs(y) > 1000000)
        throw std::runtime_error("Card type or position is outside the document limits");
    if (document.cards.size() >= maxCards || document.nextId >= 1000000000)
        throw std::runtime_error("Card limit reached");
    uint32_t id = document.nextId;
    change(
        [&](Document& d)
        {
            d.cards.push_back({d.nextId++, x, y,
                               kind == 1   ? "Decision"
                               : kind == 2 ? "Note"
                                           : "New task",
                               "Add a description in the inspector", kind,
                               kind == 1   ? 2
                               : kind == 2 ? 5
                                           : 0});
        });
    return id;
}
bool Editor::connect(uint32_t from, uint32_t to)
{
    if (from == to || !document.find(from) || !document.find(to))
        return false;
    for (const auto& e : document.connections)
        if (e.from == from && e.to == to)
            return false;
    if (document.connections.size() >= maxConnections || document.nextId >= 1000000000)
        throw std::runtime_error("Connection limit reached");
    return change([&](Document& d) { d.connections.push_back({d.nextId++, from, to}); });
}
void Editor::erase(const std::vector<uint32_t>& ids, uint32_t connection)
{
    change(
        [&](Document& d)
        {
            const auto selected = [&](uint32_t id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
            std::erase_if(d.cards, [&](const Card& c) { return selected(c.id); });
            std::erase_if(d.connections, [&](const Connection& e)
                          { return e.id == connection || selected(e.from) || selected(e.to); });
        });
}
std::vector<uint32_t> Editor::duplicate(const std::vector<uint32_t>& ids)
{
    std::vector<uint32_t> result;
    change(
        [&](Document& d)
        {
            std::map<uint32_t, uint32_t> mapping;
            const auto cards = d.cards;
            const auto edges = d.connections;
            for (const auto& card : cards)
                if (std::find(ids.begin(), ids.end(), card.id) != ids.end())
                {
                    if (d.cards.size() >= maxCards || d.nextId >= 1000000000)
                        throw std::runtime_error("Card limit reached");
                    auto copy = card;
                    copy.id = d.nextId++;
                    copy.x = std::clamp(copy.x + 30, -1000000.0f, 1000000.0f);
                    copy.y = std::clamp(copy.y + 30, -1000000.0f, 1000000.0f);
                    mapping[card.id] = copy.id;
                    result.push_back(copy.id);
                    d.cards.push_back(copy);
                }
            for (const auto& e : edges)
                if (mapping.count(e.from) && mapping.count(e.to))
                {
                    if (d.connections.size() >= maxConnections || d.nextId >= 1000000000)
                        throw std::runtime_error("Connection limit reached");
                    d.connections.push_back({d.nextId++, mapping.at(e.from), mapping.at(e.to)});
                }
        });
    return result;
}
namespace
{
constexpr std::string_view clipboardPrefix = "FLOWSTUDIO-CARDS/1\n";
std::vector<Card*> selectedCards(Document& document, const std::vector<uint32_t>& ids)
{
    std::vector<Card*> result;
    for (auto& card : document.cards)
        if (std::find(ids.begin(), ids.end(), card.id) != ids.end())
            result.push_back(&card);
    return result;
}
} // namespace
std::string copyCards(const Document& document, const std::vector<uint32_t>& ids)
{
    Document selection;
    selection.name = "Clipboard selection";
    selection.nextId = document.nextId;
    for (const auto& card : document.cards)
        if (std::find(ids.begin(), ids.end(), card.id) != ids.end())
            selection.cards.push_back(card);
    for (const auto& edge : document.connections)
        if (selection.find(edge.from) && selection.find(edge.to))
            selection.connections.push_back(edge);
    if (selection.cards.empty())
        return {};
    const auto content = serialize(selection);
    (void)parse(content);
    return std::string(clipboardPrefix) + content;
}
std::vector<uint32_t> Editor::paste(const std::string& payload, float x, float y)
{
    if (payload.size() > 2 * 1024 * 1024 + clipboardPrefix.size() || !payload.starts_with(clipboardPrefix))
        throw std::runtime_error("Clipboard does not contain a valid Flow Studio card selection");
    if (!std::isfinite(x) || !std::isfinite(y))
        throw std::runtime_error("Paste position must be finite");
    const auto selection = parse(payload.substr(clipboardPrefix.size()));
    if (selection.cards.empty())
        return {};
    if (document.cards.size() + selection.cards.size() > maxCards ||
        document.connections.size() + selection.connections.size() > maxConnections ||
        document.nextId + selection.cards.size() + selection.connections.size() > 1000000000)
        throw std::runtime_error("Paste exceeds the document card, connection or ID limits");
    float left = selection.cards.front().x, right = left, top = selection.cards.front().y, bottom = top;
    for (const auto& c : selection.cards)
    {
        left = std::min(left, c.x);
        right = std::max(right, c.x);
        top = std::min(top, c.y);
        bottom = std::max(bottom, c.y);
    }
    const float dx = std::clamp(x - left, -1000000 - left, 1000000 - right);
    const float dy = std::clamp(y - top, -1000000 - top, 1000000 - bottom);
    std::vector<uint32_t> result;
    change(
        [&](Document& d)
        {
            std::map<uint32_t, uint32_t> mapping;
            for (auto card : selection.cards)
            {
                const auto original = card.id;
                card.id = d.nextId++;
                mapping[original] = card.id;
                card.x += dx;
                card.y += dy;
                result.push_back(card.id);
                d.cards.push_back(std::move(card));
            }
            for (const auto& edge : selection.connections)
                d.connections.push_back({d.nextId++, mapping.at(edge.from), mapping.at(edge.to)});
        });
    return result;
}
bool Editor::arrange(const std::vector<uint32_t>& ids, Arrangement arrangement)
{
    return change(
        [&](Document& d)
        {
            auto cards = selectedCards(d, ids);
            const bool horizontal = arrangement == Arrangement::Left || arrangement == Arrangement::CenterX ||
                                    arrangement == Arrangement::Right || arrangement == Arrangement::DistributeX;
            const bool distribute = arrangement == Arrangement::DistributeX || arrangement == Arrangement::DistributeY;
            if (cards.size() < (distribute ? 3u : 2u))
                return;
            const auto coordinate = [&](const Card* c) { return horizontal ? c->x : c->y; };
            std::stable_sort(cards.begin(), cards.end(),
                             [&](const Card* a, const Card* b) { return coordinate(a) < coordinate(b); });
            const float first = coordinate(cards.front()), last = coordinate(cards.back());
            for (std::size_t i = 0; i < cards.size(); ++i)
            {
                // Cards have equal dimensions; equal origin spacing gives equal gaps.
                const float value =
                    distribute ? first + (last - first) * (static_cast<float>(i) / static_cast<float>(cards.size() - 1))
                    : arrangement == Arrangement::Left || arrangement == Arrangement::Top     ? first
                    : arrangement == Arrangement::Right || arrangement == Arrangement::Bottom ? last
                                                                                              : (first + last) * 0.5f;
                if (horizontal)
                    cards[i]->x = value;
                else
                    cards[i]->y = value;
            }
        });
}
bool Editor::move(const std::vector<uint32_t>& ids, float dx, float dy, bool recordHistory)
{
    if (!std::isfinite(dx) || !std::isfinite(dy))
        throw std::runtime_error("Movement must be finite");
    auto cards = selectedCards(document, ids);
    if (cards.empty())
        return false;
    float left = cards.front()->x, right = left, top = cards.front()->y, bottom = top;
    for (const auto* c : cards)
    {
        left = std::min(left, c->x);
        right = std::max(right, c->x);
        top = std::min(top, c->y);
        bottom = std::max(bottom, c->y);
    }
    dx = std::clamp(dx, -1000000 - left, 1000000 - right);
    dy = std::clamp(dy, -1000000 - top, 1000000 - bottom);
    if (dx == 0 && dy == 0)
        return false;
    if (recordHistory)
        return change(
            [&](Document&)
            {
                for (auto* c : cards)
                {
                    c->x += dx;
                    c->y += dy;
                }
            });
    for (auto* c : cards)
    {
        c->x += dx;
        c->y += dy;
    }
    return true;
}
} // namespace flow
