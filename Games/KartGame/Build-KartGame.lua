project "KartGame"
   kind "WindowedApp"
   language "C++"
   cppdialect "C++20"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

   files { "Source/**.h", "Source/**.cpp" }

   includedirs
   {
      "Source",
      "../../Core/Source",
      "../../Libs/asio-1.28.1/include",
      "../../Libs/DirectXTK/include",
      "../../Libs/DarkFireNetworking/include"
   }

   libdirs
   {
      "../../Libs/DirectXTex/bin",
      "../../Libs/DirectXTK/bin"
   }

   links
   {
      "Core",
      "ws2_32",
      "mswsock"
   }

   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")
   debugdir "."

   filter "system:windows"
       systemversion "latest"
       defines { "WINDOWS", "ASIO_STANDALONE" }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
       symbols "On"

   filter "configurations:Dist"
       defines { "DIST" }
       runtime "Release"
       optimize "On"
       symbols "Off"
