#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_opengl3.h>

#include "profiler.h"

#include <functional>
#include <Windows.h>

float gBackGroundColor[3] = { 1.0f, 1.0f, 1.0f};

void get_fps_and_ms(float& fps, float& ms);

static std::function<void()> default_layer = []()
	{
		float fps, ms;
		get_fps_and_ms(fps, ms);

		ImGui::Begin("Performance");
		ImGui::Text("Frame time: %.2f ms (%.1f FPS)", ms, fps);
		ImGui::End();

		profiler_draw_imgui();
	};

static std::function<void()> app_layer{ nullptr };

void get_fps_and_ms(float& fps, float& ms)
{
	static int frame_count = 0;
	static float last_fps = 0.0f;
	static LARGE_INTEGER last_time;
	static bool initialized = false;

	static LARGE_INTEGER frequency;
	if (!initialized) {
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&last_time);
		initialized = true;
	}

	LARGE_INTEGER current_time;
	QueryPerformanceCounter(&current_time);

	frame_count++;
	double elapsed = double(current_time.QuadPart - last_time.QuadPart) / double(frequency.QuadPart);

	if (elapsed >= 0.5) {
		last_fps = frame_count / float(elapsed);
		frame_count = 0;
		last_time = current_time;
	}

	fps = last_fps;
	ms = fps > 0.0f ? (1000.0f / fps) : 0.0f;
}

void imgui_init(void* window, const std::function<void()>& layer)
{
	HWND hwnd = static_cast<HWND>(window);
	if (layer)
		app_layer = layer;

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_InitForOpenGL(hwnd);
}

ImDrawData* imgui_get_new_frame_data()
{
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	default_layer();
	if (app_layer)
		app_layer();

	ImGui::Render();
	return ImGui::GetDrawData();
}

void imgui_terminate()
{
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
