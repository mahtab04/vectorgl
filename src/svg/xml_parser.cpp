// ============================================================================
// xml_parser.cpp — XML DOM parsing
// ============================================================================

#include <cstring>

#include "svg_common.hpp"
#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{
namespace detail
{
namespace svg
{

namespace
{

bool hasNonWhitespace(const std::string& value)
{
    for (char ch : value)
    {
        if (!isWhitespace(ch))
            return true;
    }
    return false;
}

std::string consumeTagName(const char*& cursor)
{
    std::string name;
    while (*cursor && !isWhitespace(*cursor) && *cursor != '=' && *cursor != '>' && *cursor != '/' && *cursor != '<')
    {
        name += *cursor++;
    }
    return name;
}

std::string consumeAttributeValue(const char*& cursor)
{
    char quote = *cursor++;
    std::string value;
    while (*cursor && *cursor != quote)
    {
        if (*cursor == '&')
        {
            if (std::strncmp(cursor, "&amp;", 5) == 0)
            {
                value += '&';
                cursor += 5;
            }
            else if (std::strncmp(cursor, "&lt;", 4) == 0)
            {
                value += '<';
                cursor += 4;
            }
            else if (std::strncmp(cursor, "&gt;", 4) == 0)
            {
                value += '>';
                cursor += 4;
            }
            else if (std::strncmp(cursor, "&quot;", 6) == 0)
            {
                value += '"';
                cursor += 6;
            }
            else if (std::strncmp(cursor, "&apos;", 6) == 0)
            {
                value += '\'';
                cursor += 6;
            }
            else
            {
                value += *cursor++;
            }
        }
        else
        {
            value += *cursor++;
        }
    }
    if (*cursor == quote)
        ++cursor;
    return value;
}

bool parseElementRecursive(const char*& cursor, XmlElement& out)
{
    skipWhitespace(cursor);
    if (*cursor != '<')
        return false;
    ++cursor;

    // Skip XML comments
    if (*cursor == '!')
    {
        if (std::strncmp(cursor, "!--", 3) == 0)
        {
            cursor += 3;
            while (*cursor && !(cursor[0] == '-' && cursor[1] == '-' && cursor[2] == '>'))
                ++cursor;
            if (*cursor)
                cursor += 3;
            return false;
        }
        while (*cursor && *cursor != '>')
            ++cursor;
        if (*cursor)
            ++cursor;
        return false;
    }

    // Skip processing instructions (<?xml ... ?>)
    if (*cursor == '?')
    {
        while (*cursor && !(cursor[0] == '?' && cursor[1] == '>'))
            ++cursor;
        if (*cursor)
            cursor += 2;
        return false;
    }

    out.tag = consumeTagName(cursor);
    if (out.tag.empty())
        return false;

    // Parse attributes
    while (true)
    {
        skipWhitespace(cursor);
        if (*cursor == '/' && *(cursor + 1) == '>')
        {
            cursor += 2;
            out.selfClosing = true;
            return true;
        }
        if (*cursor == '>')
        {
            ++cursor;
            break;
        }
        if (!*cursor)
            return false;

        XmlAttribute attr;
        attr.name = consumeTagName(cursor);
        skipWhitespace(cursor);
        if (*cursor == '=')
        {
            ++cursor;
            skipWhitespace(cursor);
            if (*cursor == '"' || *cursor == '\'')
            {
                attr.value = consumeAttributeValue(cursor);
            }
        }
        if (!attr.name.empty())
        {
            out.attrs.push_back(std::move(attr));
        }
    }

    // Parse children until closing tag
    while (*cursor)
    {
        skipWhitespace(cursor);
        if (!*cursor)
            break;

        if (*cursor == '<' && *(cursor + 1) == '/')
        {
            cursor += 2;
            while (*cursor && *cursor != '>')
                ++cursor;
            if (*cursor)
                ++cursor;
            return true;
        }

        if (*cursor == '<')
        {
            XmlElement child;
            if (parseElementRecursive(cursor, child))
            {
                out.children.push_back(std::move(child));
            }
        }
        else
        {
            const char* textStart = cursor;
            while (*cursor && *cursor != '<')
                ++cursor;

            std::string text(textStart, cursor - textStart);
            if (hasNonWhitespace(text))
            {
                XmlElement textNode;
                textNode.attrs.push_back({"text", text});
                out.children.push_back(std::move(textNode));
            }
        }
    }
    return true;
}

} // anonymous namespace

XmlElement parseXml(const std::string& xml)
{
    XmlElement root;
    root.tag = "__root__";
    const char* cursor = xml.c_str();
    while (*cursor)
    {
        skipWhitespace(cursor);
        if (!*cursor)
            break;
        if (*cursor == '<')
        {
            XmlElement child;
            if (parseElementRecursive(cursor, child))
            {
                root.children.push_back(std::move(child));
            }
        }
        else
        {
            ++cursor;
        }
    }
    return root;
}

const std::string& getAttribute(const XmlElement& element, const std::string& name)
{
    static const std::string kEmpty;
    for (const auto& attr : element.attrs)
    {
        if (attr.name == name)
            return attr.value;
    }
    return kEmpty;
}

float getAttributeFloat(const XmlElement& element, const std::string& name, float defaultValue)
{
    const auto& value = getAttribute(element, name);
    if (value.empty())
        return defaultValue;
    return std::strtof(value.c_str(), nullptr);
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
