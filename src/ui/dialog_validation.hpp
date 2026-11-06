#pragma once

#include <algorithm>

#include "vectorgl/detail/utf8.hpp"
#include "vectorgl/ui/file_dialogs.hpp"

namespace vectorgl::ui::detail
{
inline bool dialogText(std::string_view text)
{
    if (text.size() > 32768 || text.find('\0') != std::string_view::npos)
        return false;
    std::string normalized;
    for (std::size_t at = 0; at < text.size();)
        vectorgl::detail::appendCodepoint(normalized, vectorgl::detail::nextCodepoint(text, at));
    return normalized == text;
}
inline bool dialogOptions(const FileDialogOptions& o)
{
    if ((o.kind != FileDialogKind::Open && o.kind != FileDialogKind::Save) || !dialogText(o.title) ||
        !dialogText(o.initialDirectory) || !dialogText(o.suggestedName) || o.filters.size() > 32 ||
        o.suggestedName == "." || o.suggestedName == ".." ||
        o.suggestedName.find_first_of("<>:\"/\\|?*") != std::string::npos)
        return false;
    for (unsigned char c : o.suggestedName)
        if (c < 32 || c == 127)
            return false;
    for (const auto& filter : o.filters)
    {
        if (!dialogText(filter.label) || filter.label.empty() || filter.label.find('|') != std::string::npos ||
            filter.extensions.size() > 32)
            return false;
        for (unsigned char c : filter.label)
            if (c < 32 || c == 127)
                return false;
        for (const auto& ext : filter.extensions)
        {
            if (ext.empty() || ext.size() > 64 || ext.front() == '.' || ext.back() == '.')
                return false;
            for (unsigned char c : ext)
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                      c == '_' || c == '-'))
                    return false;
        }
    }
    return true;
}
inline FileDialogResult dialogError(std::string text)
{
    return {{ServiceStatus::Error, std::move(text)}, {}};
}
inline FileDialogResult normalizedDialogResult(FileDialogResult result)
{
    if (result.result.status != ServiceStatus::Success && result.result.status != ServiceStatus::Cancelled &&
        result.result.status != ServiceStatus::Unavailable && result.result.status != ServiceStatus::Error)
        return dialogError("Dialog returned an invalid status");
    if (result.result.status != ServiceStatus::Success)
    {
        result.path.clear();
        return result;
    }
    if (result.path.empty() || !dialogText(result.path))
        return dialogError("Dialog returned an invalid UTF-8 path");
    return result;
}
} // namespace vectorgl::ui::detail
