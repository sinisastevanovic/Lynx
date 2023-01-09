@echo off

pushd ..
set /p "project_name=Enter Project Name: "
if not exist "%project_name%\Source" mkdir %project_name%\Source
xcopy /s /f /y Lynx\ProjectSetup\Project.cpp %project_name%\Source\%project_name%.cpp*
:: Create project premake5.lua
break>%project_name%\premake5.lua
(
echo project "%project_name%"
echo    kind "ConsoleApp"
echo    language "C++"
echo    cppdialect "C++17"
echo    --targetdir "bin/%%{cfg.buildcfg}"
echo    staticruntime "off"
echo:
echo    files { "Source/**.h", "Source/**.cpp" }
echo:
echo    includedirs
echo    {
echo        "../Lynx/ThirdParty/imgui",
echo        "../Lynx/ThirdParty/glfw/include",
echo:
echo        "../Lynx/Source",
echo:
echo        "%%{IncludeDir.VulkanSDK}",
echo        "%%{IncludeDir.glm}",
echo    }
echo:
echo    links
echo    {
echo        "Lynx"
echo    }
echo:
echo    targetdir ^("../Binaries/" .. outputdir .. "/%%{prj.name}"^)
echo    objdir ^("../Intermediate/" .. outputdir .. "/%%{prj.name}"^)
echo:
echo    filter "system:windows"
echo        systemversion "latest"
echo        defines { "LX_PLATFORM_WINDOWS" }
echo:
echo    filter "configurations:Debug"
echo        defines { "LX_DEBUG" }
echo        runtime "Debug"
echo        symbols "On"
echo:
echo    filter "configurations:Release"
echo        defines { "LX_RELEASE" }
echo        runtime "Release"
echo        optimize "On"
echo        symbols "On"
echo:
echo   filter "configurations:Dist"
echo        kind "WindowedApp"
echo        defines { "LX_DIST" }
echo        runtime "Release"
echo        optimize "On"
echo        symbols "Off"
)>%project_name%\premake5.lua

:: Create base premake5.lua file
break>premake5.lua
(
    echo workspace "%project_name%"
    echo    architecture "x64"
    echo    configurations { "Debug", "Release", "Dist" }
    echo    startproject "%project_name%"
    echo:
    echo outputdir = "%%{cfg.system}-%%{cfg.architecture}-%%{cfg.buildcfg}"
    echo:
    echo include "Lynx/LynxExternal.lua"
    echo include "%project_name%"
)>premake5.lua

xcopy /s /f /y Lynx\ProjectSetup\GLFWpremake5.lua Lynx\ThirdParty\GLFW\premake5.lua*
xcopy /s /f /y Lynx\ProjectSetup\imguipremake5.lua Lynx\ThirdParty\imgui\premake5.lua*
Lynx\ThirdParty\premake5\premake5.exe vs2022
popd
pause