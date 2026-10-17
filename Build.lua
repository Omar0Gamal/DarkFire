-- premake5.lua
workspace "DarkFire"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "SandBox"

   -- Workspace-wide build options for MSVC
   filter "system:windows"
      buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }
      defines { "NOMINMAX" }

OutputDir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

group "Core"
	include "Core/Build-Core.lua"
group "Apps"
	include "SandBox/Build-App.lua"
	include "Server/Build-Server.lua"
group ""