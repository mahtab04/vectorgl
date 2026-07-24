#include "editor_app.hpp"

#include <vectorgl/canvas.hpp>

#include <iostream>

namespace vectorgl::node_editor
{

void EditorApp::init()
{
    model_.loadDemoGraph();
}

void EditorApp::shutdown()
{
}

void EditorApp::update(float deltaTime)
{
    (void)deltaTime;
}

void EditorApp::render(Canvas& canvas) const
{
    renderer_.render(canvas, model_, state_, theme_);
}

GraphModel& EditorApp::model()
{
    return model_;
}

const GraphModel& EditorApp::model() const
{
    return model_;
}

EditorState& EditorApp::state()
{
    return state_;
}

const EditorState& EditorApp::state() const
{
    return state_;
}

Theme& EditorApp::theme()
{
    return theme_;
}

const Theme& EditorApp::theme() const
{
    return theme_;
}

InputController& EditorApp::input()
{
    return input_;
}

const InputController& EditorApp::input() const
{
    return input_;
}

} // namespace vectorgl::node_editor
