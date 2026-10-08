#include <string>
#include <vectorgl/text_box.hpp>

#include "test_utils.hpp"
#include "vectorgl/detail/utf8.hpp"

int main()
{
    using namespace vectorgl;
    TextBox field;
    expect(!field.handleCharInput(0x3A9), "unfocused input rejected");
    field.setFocused(true);
    expect(field.handleCharInput('A') && field.handleCharInput(0x3A9) && field.handleCharInput(0x1F600),
           "ASCII, two-byte and four-byte characters accepted");
    const std::string omega = "\xCE\xA9", emoji = "\xF0\x9F\x98\x80";
    expect(field.text() == "A" + omega + emoji, "input encoded as UTF-8");
    expect(!field.handleCharInput(0xD800) && !field.handleCharInput(0x110000) && !field.handleCharInput('\n'),
           "invalid scalars and single-line controls rejected");
    field.handleKey(TextBoxKey::Left, true);
    expect(field.selectedText() == emoji && field.selectionStart() == 3 && field.selectionEnd() == 7,
           "selection crosses a complete supplementary codepoint");
    std::string copied;
    expect(field.copySelection(copied) && copied == emoji, "Unicode clipboard copy");
    field.handleCharInput(0x4E2D);
    expect(field.text() == "A" + omega + "\xE4\xB8\xAD", "typed Unicode replaces selection");
    field.handleKey(TextBoxKey::Backspace);
    expect(field.text() == "A" + omega, "backspace removes complete three-byte character");
    field.handleKey(TextBoxKey::Home);
    field.handleKey(TextBoxKey::Right);
    field.handleKey(TextBoxKey::Delete);
    expect(field.text() == "A", "delete removes complete two-byte character");
    field.handleKey(TextBoxKey::End);
    expect(field.pasteText(omega + emoji + "\n\t"), "Unicode paste accepted");
    expect(field.text() == "A" + omega + emoji, "paste filters single-line controls");
    field.selectAll();
    expect(!field.pasteText("\n\t") && field.hasSelection(), "empty filtered paste preserves selection");
    expect(field.cutSelection(copied) && copied == "A" + omega + emoji && field.text().empty(), "Unicode cut");
    field.setText("a\xFF"
                  "b\n");
    expect(field.text() == "a\xEF\xBF\xBD"
                           "b",
           "setText repairs invalid UTF-8 and filters controls");
    field.setText("e\xCC\x81");
    field.handleKey(TextBoxKey::Backspace);
    expect(field.text() == "e", "editing contract is codepoints rather than grapheme clusters");

    for (const auto& bad : {std::string("\xC0\xAF"), std::string("\xED\xA0\x80"), std::string("\xF4\x90\x80\x80"),
                            std::string("\xF0\x9F"), std::string("\x80")})
    {
        size_t offset = 0;
        while (offset < bad.size())
        {
            const auto before = offset;
            expect(detail::nextCodepoint(bad, offset) == 0xFFFD && offset > before,
                   "malformed UTF-8 yields replacement and makes progress");
        }
    }
    size_t offset = 0;
    expect(detail::nextCodepoint(emoji, offset) == 0x1F600 && offset == 4, "valid supplementary decoding");
}
