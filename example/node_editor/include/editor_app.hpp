#pragma once

#include "editor_state.hpp"
#include "graph_model.hpp"
#include "graph_renderer.hpp"
#include "input_controller.hpp"
#include "theme.hpp"

namespace vectorgl
{
class Canvas;
}

namespace vectorgl::node_editor
{

class EditorApp
{
public:
	EditorApp() = default;

	void init();
	void shutdown();

	void update(float deltaTime);
	void render(Canvas& canvas) const;

	GraphModel& model();
	const GraphModel& model() const;

	EditorState& state();
	const EditorState& state() const;

	Theme& theme();
	const Theme& theme() const;

	InputController& input();
	const InputController& input() const;

private:
	GraphModel model_{};
	EditorState state_{};
	Theme theme_{};
	GraphRenderer renderer_{};
	InputController input_{};
};

} // namespace vectorgl::node_editor
