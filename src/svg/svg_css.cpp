// ============================================================================
// svg_css.cpp — SVG CSS <style> block parsing
// ============================================================================

#include <algorithm>
#include <sstream>

#include "svg_common.hpp"
#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{
namespace detail
{
namespace svg
{

std::vector<CssRule> parseCssStyleBlock(const std::string& cssText)
{
    std::vector<CssRule> rules;
    size_t pos = 0;

    while (pos < cssText.size())
    {
        // Skip whitespace and comments
        while (pos < cssText.size() && isWhitespace(cssText[pos]))
            ++pos;

        // Skip CSS comments /* ... */
        if (pos + 1 < cssText.size() && cssText[pos] == '/' && cssText[pos + 1] == '*')
        {
            pos += 2;
            while (pos + 1 < cssText.size() && !(cssText[pos] == '*' && cssText[pos + 1] == '/'))
                ++pos;
            if (pos + 1 < cssText.size())
                pos += 2;
            continue;
        }

        if (pos >= cssText.size())
            break;

        // Find selector (everything before '{')
        size_t braceOpen = cssText.find('{', pos);
        if (braceOpen == std::string::npos)
            break;

        std::string selectorBlock = trimWhitespace(cssText.substr(pos, braceOpen - pos));

        // Find closing brace
        size_t braceClose = cssText.find('}', braceOpen);
        if (braceClose == std::string::npos)
            break;

        std::string declarations = trimWhitespace(cssText.substr(braceOpen + 1, braceClose - braceOpen - 1));

        // Handle comma-separated selectors: "rect, .cls1, #myId { ... }"
        std::istringstream selStream(selectorBlock);
        std::string selector;
        while (std::getline(selStream, selector, ','))
        {
            selector = trimWhitespace(selector);
            if (!selector.empty())
            {
                CssRule rule;
                rule.selector = selector;
                rule.declarations = declarations;
                rules.push_back(rule);
            }
        }

        pos = braceClose + 1;
    }

    return rules;
}

bool cssSelectorMatches(const std::string& selector, const XmlElement& element)
{
    if (selector.empty())
        return false;

    // ID selector: #myId
    if (selector[0] == '#')
    {
        const auto& elemId = getAttribute(element, "id");
        return elemId == selector.substr(1);
    }

    // Class selector: .myClass
    if (selector[0] == '.')
    {
        const auto& classAttr = getAttribute(element, "class");
        if (classAttr.empty())
            return false;
        std::string targetClass = selector.substr(1);
        // Class attr can contain multiple space-separated classes
        std::istringstream classStream(classAttr);
        std::string cls;
        while (classStream >> cls)
        {
            if (cls == targetClass)
                return true;
        }
        return false;
    }

    // Tag selector: rect, circle, path, etc.
    // Could also be "tag.class" or "tag#id" compound selectors
    size_t dotPos = selector.find('.');
    size_t hashPos = selector.find('#');

    if (dotPos != std::string::npos)
    {
        // tag.class
        std::string tagPart = selector.substr(0, dotPos);
        std::string classPart = selector.substr(dotPos + 1);
        if (!tagPart.empty() && element.tag != tagPart)
            return false;
        const auto& classAttr = getAttribute(element, "class");
        std::istringstream classStream(classAttr);
        std::string cls;
        while (classStream >> cls)
        {
            if (cls == classPart)
                return true;
        }
        return false;
    }

    if (hashPos != std::string::npos)
    {
        // tag#id
        std::string tagPart = selector.substr(0, hashPos);
        std::string idPart = selector.substr(hashPos + 1);
        if (!tagPart.empty() && element.tag != tagPart)
            return false;
        return getAttribute(element, "id") == idPart;
    }

    // Simple tag selector
    return element.tag == selector;
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
