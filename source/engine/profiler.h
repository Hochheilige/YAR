#pragma once

#include <cstdint>

// Compiled in only for the Profile configuration, which defines
// YAR_PROFILE_ENABLED (see premake5.lua). Elsewhere the macros expand to
// nothing, so no scopes are constructed and no GPU commands are recorded.

#if defined(YAR_PROFILE_ENABLED)

#include "render.h"

constexpr uint32_t kMaxCpuScopes = 32u;

struct ProfilerCpuEntry
{
	const char* name;
	double ms;
	uint32_t depth;
};

void profiler_begin_frame();
void profiler_push_cpu_scope(const char* name);
void profiler_pop_cpu_scope();

uint32_t profiler_get_cpu_entries(const ProfilerCpuEntry** out);

// Draws the "Profiler" window. Pulls GPU results from the render backend.
void profiler_draw_imgui();

struct ProfilerCpuScope
{
	explicit ProfilerCpuScope(const char* name) { profiler_push_cpu_scope(name); }
	~ProfilerCpuScope() { profiler_pop_cpu_scope(); }

	ProfilerCpuScope(const ProfilerCpuScope&) = delete;
	ProfilerCpuScope& operator=(const ProfilerCpuScope&) = delete;
};

#define YAR_CPU_SCOPE_CONCAT_(a, b) a##b
#define YAR_CPU_SCOPE_NAME(line) YAR_CPU_SCOPE_CONCAT_(cpu_scope_, line)
#define YAR_CPU_SCOPE(name) ProfilerCpuScope YAR_CPU_SCOPE_NAME(__LINE__)(name)

#define YAR_GPU_SCOPE_BEGIN(cmd, name) cmd_begin_gpu_scope(cmd, name)
#define YAR_GPU_SCOPE_END(cmd)         cmd_end_gpu_scope(cmd)

#else

inline void profiler_begin_frame() {}
inline void profiler_draw_imgui() {}

#define YAR_CPU_SCOPE(name)            ((void)0)
#define YAR_GPU_SCOPE_BEGIN(cmd, name) ((void)0)
#define YAR_GPU_SCOPE_END(cmd)         ((void)0)

#endif
