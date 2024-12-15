#pragma once

#include <functional>

extern float gBackGroundColor[3];

void imgui_init(void* window, const std::function<void()>& layer = nullptr);
void imgui_begin_frame();
void imgui_render();
void imgui_end_frame();
void imgui_terminate();