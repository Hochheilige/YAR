#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <chrono>
#include <functional>

float gBackGroundColor[3] = { 1.0f, 1.0f, 1.0f};

float get_fps();

static std::function<void()> default_layer = []()
	{
		ImGui::Begin("Performance");
		ImGui::Text("FPS: %.1f", get_fps());
		ImGui::End();
	};

static std::function<void()> app_layer{ nullptr };

float get_fps()
{
	static int frame_count = 0;
	static float fps = 0.0f;
	static auto last_time = std::chrono::high_resolution_clock::now(); 

	auto current_time = std::chrono::high_resolution_clock::now();  
	frame_count++;

	auto duration = std::chrono::duration_cast<
		std::chrono::duration<float>>(current_time - last_time);
	if (duration.count() >= 0.5f) {
		fps = frame_count / duration.count();
		last_time = current_time;
		frame_count = 0;
	}

	return fps;
}

void imgui_init(void* window, const std::function<void()>& layer)
{
	GLFWwindow* wnd = static_cast<GLFWwindow*>(window);
	if (layer)
		app_layer = layer;

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(wnd, false);
	ImGui_ImplOpenGL3_Init("#version 460");

	glfwSetMouseButtonCallback(wnd, ImGui_ImplGlfw_MouseButtonCallback);
	glfwSetScrollCallback(wnd, ImGui_ImplGlfw_ScrollCallback);
	glfwSetKeyCallback(wnd, ImGui_ImplGlfw_KeyCallback);
	glfwSetCharCallback(wnd, ImGui_ImplGlfw_CharCallback);
}

void imgui_begin_frame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void imgui_render()
{
	default_layer();
	if (app_layer)
		app_layer();
}

void imgui_end_frame()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void imgui_terminate()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}
