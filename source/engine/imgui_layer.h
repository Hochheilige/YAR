#pragma once

extern float gBackGroundColor[3];
extern int32_t gSamplesPerPixel;

void imgui_init(void* window);
void imgui_begin_frame();
void imgui_render();
void imgui_end_frame();
void imgui_terminate();