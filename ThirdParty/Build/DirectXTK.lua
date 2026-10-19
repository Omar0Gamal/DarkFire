project "DirectXTK"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	warnings "Off"

	targetdir ("../../bin/" .. OutputDir .. "/%{prj.name}")
	objdir ("../../bin-int/" .. OutputDir .. "/%{prj.name}")

	files
	{
		"../DirectXTK/Inc/**.h",
		"../DirectXTK/Inc/**.inl",
		"../DirectXTK/Src/**.h",
		"../DirectXTK/Src/**.cpp"
	}

	removefiles
	{
		"../DirectXTK/Src/XboxDDSTextureLoader.cpp"
	}

	includedirs
	{
		"../DirectXTK/Inc",
		"../DirectXTK/Src",
		"../DirectXTK/Src/Shaders/Compiled"
	}

	defines
	{
		"_UNICODE",
		"UNICODE",
		"NOMINMAX"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines { "DEBUG", "_DEBUG" }
		runtime "Debug"
		staticruntime "Off"
		symbols "On"
		editandcontinue "Off"

	filter "configurations:Debug-ASan"
		defines { "DEBUG", "_DEBUG", "DF_ASAN" }
		runtime "Debug"
		staticruntime "Off"
		symbols "On"
		editandcontinue "Off"
		buildoptions { "/fsanitize=address" }

	filter "configurations:Release"
		defines { "NDEBUG", "RELEASE" }
		runtime "Release"
		staticruntime "Off"
		optimize "On"
		symbols "On"

	filter "configurations:Dist"
		defines { "NDEBUG", "DIST" }
		runtime "Release"
		staticruntime "On"
		optimize "On"
		symbols "Off"
