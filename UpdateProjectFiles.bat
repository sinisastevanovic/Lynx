@echo off
pushd ..
if not exist Lynx\ThirdParty\GLFW\premake5.lua xcopy /s /f /y Lynx\ThirdParty\premake5\GLFWpremake5.lua Lynx\ThirdParty\GLFW\premake5.lua*
if not exist Lynx\ThirdParty\imgui\premake5.lua xcopy /s /f /y Lynx\ThirdParty\premake5\imguipremake5.lua Lynx\ThirdParty\imgui\premake5.lua*
Lynx\ThirdParty\premake5\premake5.exe vs2022
popd
pause