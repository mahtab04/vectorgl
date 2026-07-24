#pragma once

#include <vectorgl/color.hpp>

namespace vectorgl::node_editor
{

struct Theme
{
	Color backgroundColor = Color::hex(0x0B1020);
	Color gridColor = Color::hex(0x18233F, 0.75f);
	Color panelColor = Color::hex(0x131C33);
	Color panelBorderColor = Color::hex(0x2A3A63);
	Color titleColor = Color::hex(0xE2E8F0);
	Color bodyTextColor = Color::hex(0x94A3B8);
	Color accentColor = Color::hex(0x4CC9F0);
	Color selectionColor = Color::hex(0xF59E0B);
	float gridSpacing = 32.0f;
	float nodeCornerRadius = 16.0f;
	float portRadius = 7.0f;
};

} // namespace vectorgl::node_editor
