param (
    [Parameter(Mandatory=$true, Position=0)]
    [string]$ProjectName,

    [Parameter(Mandatory=$false, Position=1)]
    [ValidateSet("WindowedApp", "ConsoleApp")]
    [string]$AppKind = "WindowedApp"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$gamesDir = Join-Path $repoRoot "Games"
$targetDir = Join-Path $gamesDir $ProjectName
$sourceDir = Join-Path $targetDir "Source"

if (Test-Path $targetDir) {
    Write-Error "Project directory '$targetDir' already exists!"
    exit 1
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   DarkFire v2 Project Scaffolding: $ProjectName" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Create Directories
New-Item -ItemType Directory -Path $sourceDir -Force | Out-Null
Write-Host "[+] Created project structure: Games/$ProjectName/Source" -ForegroundColor Green

# 2. Create Build-$ProjectName.lua
$luaContent = @"
project "$ProjectName"
   kind "$AppKind"
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

   links
   {
      "Core",
      "ws2_32",
      "mswsock"
   }

   targetdir ("../../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

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
"@

$luaPath = Join-Path $targetDir "Build-$ProjectName.lua"
Set-Content -Path $luaPath -Value $luaContent -Encoding utf8
Write-Host "[+] Created Premake config: Build-$ProjectName.lua" -ForegroundColor Green

# 3. Create Source/App.cpp
$appCppContent = @"
#include "Core/All.h"
#include <iostream>

class ${ProjectName}Game : public Core::Game
{
public:
    ${ProjectName}Game(const wchar_t* title, int width, int height)
        : Core::Game(title, width, height)
    {
    }

    virtual ~${ProjectName}Game() = default;

public:
    void onCreate() override
    {
        Core::Game::onCreate();
        std::cout << "[$ProjectName] Engine initialized. Ready to build your game!\n";
    }

    void onUpdate(float deltaTime) override
    {
        Core::Game::onUpdate(deltaTime);
    }

    void onQuit() override
    {
        Core::Game::onQuit();
    }
};

#ifdef WINDOWS
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    try
    {
        ${ProjectName}Game game(L"$ProjectName", 1280, 720);
        game.run();
    }
    catch (const std::exception& e)
    {
        MessageBoxA(NULL, e.what(), "DarkFire Game Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    return 0;
}
#else
int main(int argc, char* argv[])
{
    ${ProjectName}Game game(L"$ProjectName", 1280, 720);
    game.run();
    return 0;
}
#endif
"@

$appCppPath = Join-Path $sourceDir "App.cpp"
Set-Content -Path $appCppPath -Value $appCppContent -Encoding utf8
Write-Host "[+] Created starter code: Games/$ProjectName/Source/App.cpp" -ForegroundColor Green

# 4. Run Premake to regenerate solution
Write-Host "[*] Regenerating Visual Studio solution via Premake..." -ForegroundColor Yellow
$premakeExe = Join-Path $repoRoot "Vendor\Binaries\Premake\Windows\premake5.exe"
$buildLua = Join-Path $repoRoot "Build.lua"

& $premakeExe --file=$buildLua vs2022

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n[SUCCESS] Project '$ProjectName' generated and added to DarkFire.sln!" -ForegroundColor Green
    Write-Host "Open DarkFire.sln in Visual Studio, set '$ProjectName' as StartUp Project, and press F5." -ForegroundColor Cyan
} else {
    Write-Error "Premake failed with exit code $LASTEXITCODE"
}
