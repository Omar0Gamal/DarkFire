-- DarkFire Master Premake5 Configuration
workspace "DarkFire"
	architecture "x64"
	configurations { "Debug", "Debug-ASan", "Release", "Dist" }
	startproject "Player"

	OutputDir = "%{cfg.buildcfg}-windows-%{cfg.architecture}"
	editandcontinue "Off"

	-- Global Windows configuration
	filter "system:windows"
		systemversion "latest"
		cppdialect "C++20"
		staticruntime "off"
		defines
		{
			"WIN32_LEAN_AND_MEAN",
			"NOMINMAX",
			"_UNICODE",
			"UNICODE",
			"_CRT_SECURE_NO_WARNINGS"
		}
		includedirs
		{
			"%{wks.location}/ThirdParty/DirectXTK/Inc"
		}

	filter "configurations:Dist"
		defines
		{
			"DF_STATIC"
		}

	-- Third-Party Projects
	group "ThirdParty"
		include "ThirdParty/Build/ThirdParty.lua"
	group ""

	-- Engine Core Shared Library
	group "Engine"
		project "DarkFire"
			location "Engine"
			language "C++"
			cppdialect "C++20"

			targetdir ("bin/" .. OutputDir .. "/%{prj.name}")
			objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

			files
			{
				"Engine/**.h",
				"Engine/**.hpp",
				"Engine/**.inl",
				"Engine/**.cpp",
				"ThirdParty/imgui/*.h",
				"ThirdParty/imgui/*.cpp",
				"ThirdParty/imgui/backends/imgui_impl_win32.h",
				"ThirdParty/imgui/backends/imgui_impl_win32.cpp",
				"ThirdParty/imgui/backends/imgui_impl_dx11.h",
				"ThirdParty/imgui/backends/imgui_impl_dx11.cpp",
				"ThirdParty/ImGuizmo/src/ImGuizmo.h",
				"ThirdParty/ImGuizmo/src/ImGuizmo.cpp",
				"ThirdParty/ufbx/ufbx.h",
				"ThirdParty/ufbx/ufbx.c",
				"ThirdParty/miniaudio/miniaudio.h"
			}

			includedirs
			{
				"%{wks.location}",
				"%{wks.location}/ThirdParty/entt/src",
				"%{wks.location}/ThirdParty/json/include",
				"%{wks.location}/ThirdParty/imgui",
				"%{wks.location}/ThirdParty/imgui/backends",
				"%{wks.location}/ThirdParty/ImGuizmo/src",
				"%{wks.location}/ThirdParty/tracy/public",
				"%{wks.location}/ThirdParty/cgltf",
				"%{wks.location}/ThirdParty/tinyobjloader/include",
				"%{wks.location}/ThirdParty/backward-cpp",
				"%{wks.location}/ThirdParty/DirectXTK/Inc",
				"%{wks.location}/ThirdParty/DirectXTex/DirectXTex",
				"%{wks.location}/ThirdParty/ufbx",
				"%{wks.location}/ThirdParty/miniaudio"
			}

			links
			{
				"DirectXTK",
				"DirectXTex",
				"d3d11",
				"dxgi",
				"d3dcompiler",
				"dxguid",
				"dbghelp",
				"version"
			}

			warnings "Extra"
			buildoptions { "/WX", "/MP", "/utf-8", "/permissive-", "/Zc:preprocessor", "/Zc:__cplusplus", "/fp:precise", "/wd4100", "/wd4251" }

			filter "files:ThirdParty/**"
				warnings "Off"
				buildoptions { "/WX-" }

			filter "configurations:not Dist"
				kind "SharedLib"
				defines
				{
					"DF_BUILD_DLL",
					"ENTT_API_EXPORT",
					"DF_EDITOR",
					"IMGUI_API=__declspec(dllexport)"
				}
				postbuildcommands
				{
					"{MKDIR} \"%{wks.location}/bin/" .. OutputDir .. "/Player\"",
					"{COPYFILE} \"%{cfg.buildtarget.relpath}\" \"%{wks.location}/bin/" .. OutputDir .. "/Player/%{cfg.buildtarget.name}\"",
					"{MKDIR} \"%{wks.location}/bin/" .. OutputDir .. "/Editor\"",
					"{COPYFILE} \"%{cfg.buildtarget.relpath}\" \"%{wks.location}/bin/" .. OutputDir .. "/Editor/%{cfg.buildtarget.name}\"",
					"{MKDIR} \"%{wks.location}/bin/" .. OutputDir .. "/Tests\"",
					"{COPYFILE} \"%{cfg.buildtarget.relpath}\" \"%{wks.location}/bin/" .. OutputDir .. "/Tests/%{cfg.buildtarget.name}\""
				}

			filter "configurations:Dist"
				kind "StaticLib"
				staticruntime "on"
				defines
				{
					"DF_STATIC",
					"ENTT_API_EXPORT"
				}

			filter "configurations:Debug"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG", "TRACY_ENABLE" }
				runtime "Debug"
				symbols "On"

			filter "configurations:Debug-ASan"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG", "DF_ASAN", "TRACY_ENABLE" }
				runtime "Debug"
				symbols "On"
				buildoptions { "/fsanitize=address" }
				links { "clang_rt.asan_dynamic-x86_64.lib", "clang_rt.asan_dynamic_runtime_thunk-x86_64.lib" }

			filter "configurations:Release"
				defines { "NDEBUG", "RELEASE", "DF_RELEASE", "TRACY_ENABLE" }
				runtime "Release"
				optimize "On"
				symbols "On"

			filter "configurations:Dist"
				defines { "NDEBUG", "DIST", "DF_DIST" }
				runtime "Release"
				optimize "On"
				symbols "Off"
	group ""

	-- Standalone Player Runtime Host
	group "Runtime"
		project "Player"
			location "Player"
			kind "ConsoleApp"
			language "C++"
			cppdialect "C++20"

			targetdir ("bin/" .. OutputDir .. "/%{prj.name}")
			objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

			files
			{
				"Player/**.h",
				"Player/**.cpp"
			}

			includedirs
			{
				"%{wks.location}",
				"%{wks.location}/ThirdParty/entt/src",
				"%{wks.location}/ThirdParty/json/include"
			}

			links
			{
				"DarkFire"
			}

			warnings "Extra"
			buildoptions { "/WX", "/MP", "/utf-8", "/permissive-", "/Zc:preprocessor", "/Zc:__cplusplus", "/fp:precise", "/wd4100", "/wd4251" }

			defines
			{
				"ENTT_API_IMPORT"
			}

			filter "configurations:Debug"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG" }
				runtime "Debug"
				symbols "On"

			filter "configurations:Debug-ASan"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG", "DF_ASAN" }
				runtime "Debug"
				symbols "On"
				buildoptions { "/fsanitize=address" }
				links { "clang_rt.asan_dynamic-x86_64.lib", "clang_rt.asan_dynamic_runtime_thunk-x86_64.lib" }

			filter "configurations:Release"
				defines { "NDEBUG", "RELEASE", "DF_RELEASE" }
				runtime "Release"
				optimize "On"
				symbols "On"
				kind "WindowedApp"
				entrypoint "mainCRTStartup"

			filter "configurations:Dist"
				defines { "NDEBUG", "DIST", "DF_DIST" }
				staticruntime "on"
				runtime "Release"
				optimize "On"
				symbols "Off"
				kind "WindowedApp"
				entrypoint "mainCRTStartup"
	group ""

	-- Standalone Editor
	group "Editor"
		project "Editor"
			location "Editor"
			kind "ConsoleApp"
			language "C++"
			cppdialect "C++20"
			removeconfigurations { "Dist" }

			targetdir ("bin/" .. OutputDir .. "/%{prj.name}")
			objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

			files
			{
				"Editor/**.h",
				"Editor/**.cpp"
			}

			includedirs
			{
				"%{wks.location}",
				"%{wks.location}/ThirdParty/entt/src",
				"%{wks.location}/ThirdParty/json/include",
				"%{wks.location}/ThirdParty/imgui",
				"%{wks.location}/ThirdParty/imgui/backends",
				"%{wks.location}/ThirdParty/ImGuizmo/src"
			}

			links
			{
				"DarkFire"
			}

			warnings "Extra"
			buildoptions { "/WX", "/MP", "/utf-8", "/permissive-", "/Zc:preprocessor", "/Zc:__cplusplus", "/fp:precise", "/wd4100", "/wd4251" }

			defines
			{
				"ENTT_API_IMPORT",
				"DF_EDITOR",
				"IMGUI_API=__declspec(dllimport)"
			}

			filter "configurations:Debug"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG" }
				runtime "Debug"
				symbols "On"

			filter "configurations:Debug-ASan"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG", "DF_ASAN" }
				runtime "Debug"
				symbols "On"
				buildoptions { "/fsanitize=address" }
				links { "clang_rt.asan_dynamic-x86_64.lib", "clang_rt.asan_dynamic_runtime_thunk-x86_64.lib" }

			filter "configurations:Release"
				defines { "NDEBUG", "RELEASE", "DF_RELEASE" }
				runtime "Release"
				optimize "On"
				symbols "On"
	group ""

	-- Unit Tests Suite
	group "Tests"
		project "Tests"
			location "Tests"
			kind "ConsoleApp"
			language "C++"
			cppdialect "C++20"
			removeconfigurations { "Dist" }

			targetdir ("bin/" .. OutputDir .. "/%{prj.name}")
			objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

			files
			{
				"Tests/Source/**.h",
				"Tests/Source/**.cpp"
			}

			includedirs
			{
				"%{wks.location}",
				"%{wks.location}/ThirdParty/doctest",
				"%{wks.location}/ThirdParty/entt/src",
				"%{wks.location}/ThirdParty/json/include",
				"%{wks.location}/ThirdParty/imgui",
				"%{wks.location}/ThirdParty/imgui/backends"
			}

			links
			{
				"DarkFire"
			}

			warnings "Extra"
			buildoptions { "/WX", "/MP", "/utf-8", "/permissive-", "/Zc:preprocessor", "/Zc:__cplusplus", "/fp:precise", "/wd4100", "/wd4251", "/wd5285" }

			defines
			{
				"ENTT_API_IMPORT",
				"DF_TESTS",
				"IMGUI_API=__declspec(dllimport)"
			}

			filter "configurations:Debug"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG" }
				runtime "Debug"
				symbols "On"

			filter "configurations:Debug-ASan"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG", "DF_ASAN" }
				runtime "Debug"
				symbols "On"
				buildoptions { "/fsanitize=address" }
				links { "clang_rt.asan_dynamic-x86_64.lib", "clang_rt.asan_dynamic_runtime_thunk-x86_64.lib" }

			filter "configurations:Release"
				defines { "NDEBUG", "RELEASE", "DF_RELEASE" }
				runtime "Release"
				optimize "On"
				symbols "On"

			filter "configurations:Dist"
				defines { "NDEBUG", "DIST", "DF_DIST" }
				staticruntime "on"
				runtime "Release"
				optimize "On"
				symbols "Off"
	group ""

	-- Playground3D Reference Template Module
	group "Templates"
		project "Playground3D"
			location "Templates/Playground3D"
			language "C++"
			cppdialect "C++20"

			targetdir ("bin/" .. OutputDir .. "/Templates/%{prj.name}")
			objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

			files
			{
				"Templates/Playground3D/Source/**.h",
				"Templates/Playground3D/Source/**.cpp"
			}

			includedirs
			{
				"%{wks.location}",
				"%{wks.location}/ThirdParty/entt/src",
				"%{wks.location}/ThirdParty/json/include"
			}

			links
			{
				"DarkFire"
			}

			warnings "Extra"
			buildoptions { "/WX", "/MP", "/utf-8", "/permissive-", "/Zc:preprocessor", "/Zc:__cplusplus", "/fp:precise", "/wd4100", "/wd4251" }

			defines
			{
				"ENTT_API_IMPORT",
				"DF_MODULE_EXPORT"
			}

			filter "configurations:not Dist"
				kind "SharedLib"

			filter "configurations:Dist"
				kind "StaticLib"
				staticruntime "on"

			filter "configurations:Debug"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG" }
				runtime "Debug"
				symbols "On"

			filter "configurations:Debug-ASan"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG", "DF_ASAN" }
				runtime "Debug"
				symbols "On"
				buildoptions { "/fsanitize=address" }
				links { "clang_rt.asan_dynamic-x86_64.lib", "clang_rt.asan_dynamic_runtime_thunk-x86_64.lib" }

			filter "configurations:Release"
				defines { "NDEBUG", "RELEASE", "DF_RELEASE" }
				runtime "Release"
				optimize "On"
				symbols "On"

			filter "configurations:Dist"
				defines { "NDEBUG", "DIST", "DF_DIST" }
				runtime "Release"
				optimize "On"
				symbols "Off"

		project "Empty3D"
			location "Templates/Empty3D"
			language "C++"
			cppdialect "C++20"

			targetdir ("bin/" .. OutputDir .. "/Templates/%{prj.name}")
			objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

			files
			{
				"Templates/Empty3D/Source/**.h",
				"Templates/Empty3D/Source/**.cpp"
			}

			includedirs
			{
				"%{wks.location}",
				"%{wks.location}/ThirdParty/entt/src",
				"%{wks.location}/ThirdParty/json/include"
			}

			links
			{
				"DarkFire"
			}

			warnings "Extra"
			buildoptions { "/WX", "/MP", "/utf-8", "/permissive-", "/Zc:preprocessor", "/Zc:__cplusplus", "/fp:precise", "/wd4100", "/wd4251" }

			defines
			{
				"ENTT_API_IMPORT",
				"DF_MODULE_EXPORT"
			}

			filter "configurations:not Dist"
				kind "SharedLib"

			filter "configurations:Dist"
				kind "StaticLib"
				staticruntime "on"

			filter "configurations:Debug"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG" }
				runtime "Debug"
				symbols "On"

			filter "configurations:Debug-ASan"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG", "DF_ASAN" }
				runtime "Debug"
				symbols "On"
				buildoptions { "/fsanitize=address" }
				links { "clang_rt.asan_dynamic-x86_64.lib", "clang_rt.asan_dynamic_runtime_thunk-x86_64.lib" }

			filter "configurations:Release"
				defines { "NDEBUG", "RELEASE", "DF_RELEASE" }
				runtime "Release"
				optimize "On"
				symbols "On"

			filter "configurations:Dist"
				defines { "NDEBUG", "DIST", "DF_DIST" }
				runtime "Release"
				optimize "On"
				symbols "Off"

		project "GraphicsTest3D"
			location "Templates/GraphicsTest3D"
			language "C++"
			cppdialect "C++20"

			targetdir ("bin/" .. OutputDir .. "/Templates/%{prj.name}")
			objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

			files
			{
				"Templates/GraphicsTest3D/Source/**.h",
				"Templates/GraphicsTest3D/Source/**.cpp"
			}

			includedirs
			{
				"%{wks.location}",
				"%{wks.location}/ThirdParty/entt/src",
				"%{wks.location}/ThirdParty/json/include"
			}

			links
			{
				"DarkFire"
			}

			warnings "Extra"
			buildoptions { "/WX", "/MP", "/utf-8", "/permissive-", "/Zc:preprocessor", "/Zc:__cplusplus", "/fp:precise", "/wd4100", "/wd4251" }

			defines
			{
				"ENTT_API_IMPORT",
				"DF_MODULE_EXPORT"
			}

			filter "configurations:not Dist"
				kind "SharedLib"

			filter "configurations:Dist"
				kind "StaticLib"
				staticruntime "on"

			filter "configurations:Debug"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG" }
				runtime "Debug"
				symbols "On"

			filter "configurations:Debug-ASan"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG", "DF_ASAN" }
				runtime "Debug"
				symbols "On"
				buildoptions { "/fsanitize=address" }
				links { "clang_rt.asan_dynamic-x86_64.lib", "clang_rt.asan_dynamic_runtime_thunk-x86_64.lib" }

			filter "configurations:Release"
				defines { "NDEBUG", "RELEASE", "DF_RELEASE" }
				runtime "Release"
				optimize "On"
				symbols "On"

			filter "configurations:Dist"
				defines { "NDEBUG", "DIST", "DF_DIST" }
				runtime "Release"
				optimize "On"
				symbols "Off"
	group ""

	-- CLI Tools
	group "Tools"
		project "dfnew"
			location "Tools/dfnew"
			kind "ConsoleApp"
			language "C++"
			cppdialect "C++20"

			targetdir ("bin/" .. OutputDir .. "/Tools/%{prj.name}")
			objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

			files
			{
				"Tools/dfnew/**.h",
				"Tools/dfnew/**.cpp"
			}

			warnings "Extra"
			buildoptions { "/WX", "/MP", "/utf-8", "/permissive-", "/Zc:preprocessor", "/Zc:__cplusplus", "/fp:precise", "/wd4100", "/wd4251" }

			filter "configurations:Debug"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG" }
				runtime "Debug"
				symbols "On"

			filter "configurations:Debug-ASan"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG", "DF_ASAN" }
				runtime "Debug"
				symbols "On"
				buildoptions { "/fsanitize=address" }
				links { "clang_rt.asan_dynamic-x86_64.lib", "clang_rt.asan_dynamic_runtime_thunk-x86_64.lib" }

			filter "configurations:Release"
				defines { "NDEBUG", "RELEASE", "DF_RELEASE" }
				runtime "Release"
				optimize "On"
				symbols "On"

			filter "configurations:Dist"
				defines { "NDEBUG", "DIST", "DF_DIST" }
				staticruntime "on"
				runtime "Release"
				optimize "On"
				symbols "Off"

		project "dfcook"
			location "Tools/dfcook"
			kind "ConsoleApp"
			language "C++"
			cppdialect "C++20"

			targetdir ("bin/" .. OutputDir .. "/Tools/%{prj.name}")
			objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

			files
			{
				"Tools/dfcook/**.h",
				"Tools/dfcook/**.cpp"
			}

			includedirs
			{
				"%{wks.location}",
				"%{wks.location}/ThirdParty/DirectXTex/DirectXTex"
			}

			links
			{
				"d3dcompiler",
				"dxguid",
				"DirectXTex"
			}

			warnings "Extra"
			buildoptions { "/WX", "/MP", "/utf-8", "/permissive-", "/Zc:preprocessor", "/Zc:__cplusplus", "/fp:precise", "/wd4100", "/wd4251" }

			filter "configurations:Debug"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG" }
				runtime "Debug"
				symbols "On"

			filter "configurations:Debug-ASan"
				defines { "DEBUG", "_DEBUG", "DF_DEBUG", "DF_ASAN" }
				runtime "Debug"
				symbols "On"
				buildoptions { "/fsanitize=address" }
				links { "clang_rt.asan_dynamic-x86_64.lib", "clang_rt.asan_dynamic_runtime_thunk-x86_64.lib" }

			filter "configurations:Release"
				defines { "NDEBUG", "RELEASE", "DF_RELEASE" }
				runtime "Release"
				optimize "On"
				symbols "On"

			filter "configurations:Dist"
				defines { "NDEBUG", "DIST", "DF_DIST" }
				staticruntime "on"
				runtime "Release"
				optimize "On"
				symbols "Off"
	group ""

