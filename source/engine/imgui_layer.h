#pragma once

#include <functional>

extern float gBackGroundColor[3];

void imgui_init(void* window, const std::function<void()>& layer = nullptr);
struct ImDrawData;
ImDrawData* imgui_get_new_frame_data();
void imgui_terminate();