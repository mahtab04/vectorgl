// Headless event-routing example. Rectangles are supplied by the application;
// widget painting, a GLFW UI adapter and automatic layout are subsequent slices.
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vectorgl/text_box.hpp>
#include <vectorgl/ui/context.hpp>

using namespace vectorgl::ui;

int main()
{
    try
    {
        // Keep user-owned callback state alive longer than Context registrations.
        vectorgl::TextBox title, description;
        title.setText("Pump");
        int activations = 0, releases = 0, artboardDeletes = 0;
        Context ui;
        auto makeField = [&](vectorgl::TextBox& editor, float y)
        {
            Widget widget;
            widget.bounds = {0, y, 240, 40};
            widget.focusable = true;
            widget.onEvent = [&editor](Context&, WidgetId, const Event& event)
            {
                if (event.type == EventType::FocusGained)
                    editor.setFocused(true);
                if (event.type == EventType::FocusLost)
                    editor.setFocused(false);
                if (event.type == EventType::PointerDown)
                    return EventReply{true, true};
                if (event.type == EventType::TextInput)
                {
                    editor.pasteText(event.text);
                    return EventReply{true};
                }
                if ((event.type == EventType::KeyDown || event.type == EventType::KeyRepeat) &&
                    event.key == Key::Delete)
                {
                    editor.handleKey(vectorgl::TextBoxKey::Delete);
                    return EventReply{true}; // Consume even when caret is at the end.
                }
                return EventReply{};
            };
            return ui.add(std::move(widget));
        };
        const auto first = makeField(title, 0);
        const auto second = makeField(description, 50);
        Widget button;
        button.bounds = {0, 100, 100, 40};
        button.focusable = true;
        button.onEvent = [&](Context&, WidgetId, const Event& event)
        {
            if (event.key == Key::Enter && event.type == EventType::KeyDown)
            {
                ++activations;
                return EventReply{true};
            }
            return EventReply{};
        };
        const auto buttonId = ui.add(button);
        Widget handle;
        handle.bounds = {260, 0, 12, 140};
        handle.onEvent = [&](Context&, WidgetId, const Event& event)
        {
            if (event.type == EventType::PointerDown)
                return EventReply{true, false, true};
            if (event.type == EventType::PointerUp)
                ++releases;
            return EventReply{event.type == EventType::PointerMove || event.type == EventType::PointerUp};
        };
        ui.add(handle);
        auto key = [&](Key value, bool shift = false)
        {
            Event event;
            event.type = EventType::KeyDown;
            event.key = value;
            event.modifiers.shift = shift;
            const bool handled = ui.dispatch(event);
            if (!handled && value == Key::Delete)
                ++artboardDeletes;
        };
        key(Key::Tab);
        if (ui.focused() != first)
            throw std::runtime_error("first field was not focused");
        key(Key::Delete); // No-op in the field, not a delete-object command.
        key(Key::Tab);
        Event text;
        text.type = EventType::TextInput;
        text.text = "Unicode: Caf\xc3\xa9";
        ui.dispatch(text);
        if (ui.focused() != second || description.text() != text.text)
            throw std::runtime_error("committed UTF-8 text was misrouted");
        key(Key::Tab);
        key(Key::Enter);
        if (ui.focused() != buttonId || activations != 1)
            throw std::runtime_error("keyboard activation failed");
        key(Key::Tab, true);
        Event drag;
        drag.type = EventType::PointerDown;
        drag.x = 265;
        drag.y = 20;
        drag.button = PointerButton::Primary;
        ui.dispatch(drag);
        drag.type = EventType::PointerMove;
        drag.x = 800; // Outside the handle, still inside the hypothetical host window.
        ui.dispatch(drag);
        drag.type = EventType::PointerUp;
        ui.dispatch(drag);
        const auto modal = ui.add(Widget{});
        ui.pushModal(modal);
        key(Key::Delete); // Empty modal still blocks underlying commands.
        ui.popModal();
        ui.remove(second); // Focus transfers safely; no dangling editor reference.
        if (artboardDeletes != 0 || releases != 1 || ui.captured())
            throw std::runtime_error("input escaped its owner");
        std::cout << "Field: " << description.text() << '\n'
                  << "Button activations: " << activations << '\n'
                  << "Captured drag completions: " << releases << '\n'
                  << "Artboard Delete commands: " << artboardDeletes << '\n'
                  << "Routing, modal isolation and widget removal passed.\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
