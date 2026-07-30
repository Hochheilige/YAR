#include "profiler.h"

#if defined(YAR_PROFILE_ENABLED)

#include "render.h"

#include <imgui.h>

#define NOMINMAX
#include <Windows.h>

namespace
{

// Raw per-frame numbers are unreadable at a few hundred fps, so every entry is
// exponentially smoothed. Lower alpha = steadier but slower to react.
constexpr double kSmoothAlpha = 0.03;

struct CpuStackEntry
{
	LARGE_INTEGER start;
	uint32_t entry_index;
};

ProfilerCpuEntry entries[kMaxCpuScopes]{};
uint32_t entry_count = 0;

// The imgui layer is built before the later scopes close, so the UI reads a
// snapshot of the previous completed frame.
ProfilerCpuEntry display_entries[kMaxCpuScopes]{};
uint32_t display_count = 0;

CpuStackEntry stack[kMaxCpuScopes]{};
uint32_t stack_depth = 0;

LARGE_INTEGER frequency{};
bool initialized = false;

void ensure_initialized()
{
	if (initialized)
		return;

	QueryPerformanceFrequency(&frequency);
	initialized = true;
}

double smooth(double previous, double current, bool reset)
{
	if (reset || previous <= 0.0)
		return current;

	return previous + (current - previous) * kSmoothAlpha;
}

} // namespace

void profiler_begin_frame()
{
	ensure_initialized();

	// Publish the frame that just finished before resetting.
	for (uint32_t i = 0; i < entry_count; ++i)
		display_entries[i] = entries[i];
	display_count = entry_count;

	// Entry slots keep their smoothed values; only the count resets, so a
	// stable scope order means index i always refers to the same scope.
	entry_count = 0;
	stack_depth = 0;
}

void profiler_push_cpu_scope(const char* name)
{
	ensure_initialized();

	if (entry_count >= kMaxCpuScopes || stack_depth >= kMaxCpuScopes)
		return;

	const uint32_t index = entry_count++;

	ProfilerCpuEntry& entry = entries[index];
	const bool changed = entry.name != name;
	entry.name = name;
	entry.depth = stack_depth;
	if (changed)
		entry.ms = 0.0;

	CpuStackEntry& stack_entry = stack[stack_depth++];
	stack_entry.entry_index = index;
	QueryPerformanceCounter(&stack_entry.start);
}

void profiler_pop_cpu_scope()
{
	if (stack_depth == 0)
		return;

	const CpuStackEntry& stack_entry = stack[--stack_depth];

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	const double elapsed_ms =
		double(now.QuadPart - stack_entry.start.QuadPart) * 1000.0 / double(frequency.QuadPart);

	ProfilerCpuEntry& entry = entries[stack_entry.entry_index];
	entry.ms = smooth(entry.ms, elapsed_ms, false);
}

uint32_t profiler_get_cpu_entries(const ProfilerCpuEntry** out)
{
	*out = display_entries;
	return display_count;
}

void profiler_draw_imgui()
{
	const ProfilerCpuEntry* cpu_entries = nullptr;
	const uint32_t cpu_count = profiler_get_cpu_entries(&cpu_entries);

	yar_gpu_scope_result gpu_results[kMaxGpuScopes];
	const uint32_t gpu_count = get_gpu_scope_results(gpu_results, kMaxGpuScopes);

	ImGui::Begin("Profiler");

	auto draw_rows = [](const char* label, uint32_t count, auto&& get_row)
	{
		if (count == 0)
		{
			ImGui::TextDisabled("%s: no data", label);
			return;
		}

		if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
			return;

		if (!ImGui::BeginTable(label, 2, ImGuiTableFlags_SizingFixedFit))
			return;

		ImGui::TableSetupColumn("scope", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);

		for (uint32_t i = 0; i < count; ++i)
		{
			const char* name = nullptr;
			double ms = 0.0;
			uint32_t depth = 0;
			get_row(i, name, ms, depth);

			if (name == nullptr)
				continue;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%*s%s", int(depth) * 2, "", name);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%6.3f", ms);
		}

		ImGui::EndTable();
	};

	draw_rows("CPU", cpu_count,
		[&](uint32_t i, const char*& name, double& ms, uint32_t& depth)
		{
			name = cpu_entries[i].name;
			ms = cpu_entries[i].ms;
			depth = cpu_entries[i].depth;
		});

	draw_rows("GPU", gpu_count,
		[&](uint32_t i, const char*& name, double& ms, uint32_t& depth)
		{
			name = gpu_results[i].name;
			ms = gpu_results[i].ms;
			depth = gpu_results[i].depth;
		});

	ImGui::End();
}

#endif
