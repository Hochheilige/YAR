
toolset "msc"
filter { "system:windows", "kind:StaticLib" }
    systemversion "latest"

workspace "Yet_Another_Renderer"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "Application"

    outputdir = "build/%{cfg.architecture}/%{cfg.buildcfg}/output"

project "Engine"
    location "makefiles"
    kind "StaticLib"
    language "C++"
    compileas "C++"
    cppdialect "C++latest"
    staticruntime "off"

    debugdir (outputdir)
    targetdir ("build/%{cfg.architecture}/%{cfg.buildcfg}/lib")
    objdir ("build/%{cfg.architecture}/%{cfg.buildcfg}/intermediate")

    files {
        "source/engine/**.cpp",
        "source/engine/render_impl/**.cpp",
        "external/glad/src/**.c",
        "external/imgui/imgui.cpp",
        "external/imgui/imgui_draw.cpp",
        "external/imgui/imgui_demo.cpp",
        "external/imgui/imgui_tables.cpp",
        "external/imgui/imgui_widgets.cpp",
        "external/imgui/backends/imgui_impl_glfw.cpp",
        "external/imgui/backends/imgui_impl_opengl3.cpp",
        "external/spirv-reflect/spirv_reflect.cpp",
        "external/spirv-cross/spirv_parser.cpp",
        "external/spirv-cross/spirv_glsl.cpp",
        "external/spirv-cross/spirv_cross.cpp",
        "external/spirv-cross/spirv_cross_util.cpp",
        "external/spirv-cross/spirv_cross_parsed_ir.cpp",
        "external/spirv-cross/spirv_cfg.cpp",
        "source/engine/**.h",
    }

    includedirs {
        "external/glad/include",
        "external/glfw/include",
        "external/imgui",
        "external/spirv-reflect",
        "external/spirv-cross",
    }

    filter { "configurations:Debug" }
        libdirs {
            "external/lib/Debug"
        }
        links {
            "opengl32",
            "glfw3",
            "dxcompiler",
            "zlibstaticd",
            "assimp-vc143-mtd",
            "meshoptimizer",
        }
        symbols "On"
        runtime "Debug"

    filter { "configurations:Release" }
        libdirs {
            "external/lib/Release"
        }
        links {
            "opengl32",
            "glfw3",
            "dxcompiler",
            "zlibstatic",
            "assimp-vc143-mt"
        }
        optimize "On"
        runtime "Release"

project "Application"
    location "makefiles"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    staticruntime "off"

    targetdir (outputdir)
    debugdir (outputdir)
    objdir ("build/%{cfg.architecture}/%{cfg.buildcfg}/intermediate")
    targetname "Application"

    files {
        "source/application/app.cpp"
    }

    includedirs {
        "source/engine/",
        "external/glad/include",
        "external/glfw/include",
        "external/imgui",
        "external/glm",
        "external/stb",
        "external/assimp/include",
        "external/assimp/build/include",
        "external/meshoptimizer/src",
    }

    libdirs {
        "build/%{cfg.architecture}/%{cfg.buildcfg}/lib"
    }
    links {
        "Engine",
    }

    filter { "configurations:Debug" }
        symbols "On"
        runtime "Debug"
        postbuildcommands {
            "py \"%{prj.location}/scripts/compile_hlsl_to_spirv.py\" \"%{wks.location}/source/shaders\" \"%{cfg.targetdir}/shaders\"",
            "{COPY} \"%{wks.location}/assets\" \"%{cfg.targetdir}/assets\""
        }

    filter { "configurations:Release" }
        optimize "On"
        runtime "Release"
        postbuildcommands {
            "py \"%{prj.location}/scripts/compile_hlsl_to_spirv.py\" \"%{wks.location}/source/shaders\" \"%{cfg.targetdir}/shaders\"",
            "{COPY} \"%{wks.location}/assets\" \"%{cfg.targetdir}/assets\""
        }

        
