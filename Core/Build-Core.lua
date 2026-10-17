project "Core"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

   files { "Source/**.h", "Source/**.cpp" }

   includedirs
   {
      "Source",

      -- Include Libraries
      "../Libs/DirectXTex/include",
      "../Libs/DirectXTK/include",
      "../Libs/StackTrace",
      "../Libs/tinyobjloader/include"
   }

   libdirs { "../Libs/DirectXTex/bin", "../Libs/DirectXTK/bin" }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines { }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"
       links { "DirectXTexD", "DirectXTKD", "d3d11", "dxgi", "d3dcompiler" }

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
       symbols "On"
       links { "DirectXTex", "DirectXTK", "d3d11", "dxgi", "d3dcompiler" }

   filter "configurations:Dist"
       defines { "DIST" }
       runtime "Release"
       optimize "On"
       symbols "Off"
       links { "DirectXTex", "DirectXTK", "d3d11", "dxgi", "d3dcompiler" }