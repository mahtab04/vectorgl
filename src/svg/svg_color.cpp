// ============================================================================
// svg_color.cpp — SVG color string parsing
// ============================================================================

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{
namespace detail
{
namespace svg
{

namespace
{

const std::unordered_map<std::string, uint32_t>& namedColorMap()
{
    static const std::unordered_map<std::string, uint32_t> table = {
        {"black", 0x000000},       {"white", 0xFFFFFF},        {"red", 0xFF0000},         {"green", 0x008000},
        {"blue", 0x0000FF},        {"yellow", 0xFFFF00},       {"cyan", 0x00FFFF},        {"magenta", 0xFF00FF},
        {"orange", 0xFFA500},      {"purple", 0x800080},       {"pink", 0xFFC0CB},        {"brown", 0xA52A2A},
        {"gray", 0x808080},        {"grey", 0x808080},         {"darkgray", 0xA9A9A9},    {"darkgrey", 0xA9A9A9},
        {"lightgray", 0xD3D3D3},   {"lightgrey", 0xD3D3D3},    {"navy", 0x000080},        {"teal", 0x008080},
        {"olive", 0x808000},       {"maroon", 0x800000},       {"aqua", 0x00FFFF},        {"lime", 0x00FF00},
        {"silver", 0xC0C0C0},      {"fuchsia", 0xFF00FF},      {"coral", 0xFF7F50},       {"tomato", 0xFF6347},
        {"gold", 0xFFD700},        {"indigo", 0x4B0082},       {"violet", 0xEE82EE},      {"crimson", 0xDC143C},
        {"turquoise", 0x40E0D0},   {"salmon", 0xFA8072},       {"khaki", 0xF0E68C},       {"plum", 0xDDA0DD},
        {"orchid", 0xDA70D6},      {"tan", 0xD2B48C},          {"chocolate", 0xD2691E},   {"firebrick", 0xB22222},
        {"darkred", 0x8B0000},     {"darkgreen", 0x006400},    {"darkblue", 0x00008B},    {"steelblue", 0x4682B4},
        {"royalblue", 0x4169E1},   {"dodgerblue", 0x1E90FF},   {"deepskyblue", 0x00BFFF}, {"skyblue", 0x87CEEB},
        {"lightblue", 0xADD8E6},   {"midnightblue", 0x191970}, {"slategray", 0x708090},   {"dimgray", 0x696969},
        {"whitesmoke", 0xF5F5F5},  {"limegreen", 0x32CD32},    {"forestgreen", 0x228B22}, {"seagreen", 0x2E8B57},
        {"springgreen", 0x00FF7F}, {"orangered", 0xFF4500},    {"darkorange", 0xFF8C00},  {"hotpink", 0xFF69B4},
        {"deeppink", 0xFF1493},
    };
    return table;
}

} // anonymous namespace

Color parseColor(const std::string& str)
{
    if (str.empty() || str == "none")
        return Color::Transparent;

    if (str[0] == '#')
    {
        uint32_t hex = 0;
        if (str.size() == 7)
        {
            hex = static_cast<uint32_t>(std::strtoul(str.c_str() + 1, nullptr, 16));
        }
        else if (str.size() == 4)
        {
            char expanded[7];
            expanded[0] = str[1];
            expanded[1] = str[1];
            expanded[2] = str[2];
            expanded[3] = str[2];
            expanded[4] = str[3];
            expanded[5] = str[3];
            expanded[6] = '\0';
            hex = static_cast<uint32_t>(std::strtoul(expanded, nullptr, 16));
        }
        return Color::hex(hex);
    }

    if (str.size() > 4 && str[0] == 'r' && str[1] == 'g' && str[2] == 'b' && str[3] == '(')
    {
        int r = 0, g = 0, b = 0;
        std::sscanf(str.c_str(), "rgb(%d,%d,%d)", &r, &g, &b);
        return Color::rgba(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
    }

    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto it = namedColorMap().find(lower);
    if (it != namedColorMap().end())
        return Color::hex(it->second);

    return Color::Black;
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
