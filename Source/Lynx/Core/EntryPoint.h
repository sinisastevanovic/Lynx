#pragma once

#ifdef LX_PLATFORM_WINDOWS

extern Lynx::Application* Lynx::CreateApplication(int argc, char** argv);
bool g_ApplicationRunning = true;

namespace Lynx
{
    int Main(int argc, char** argv)
    {
        while (g_ApplicationRunning)
        {
            Lynx::Application* app = Lynx::CreateApplication(argc, argv);
            app->Run();
            delete app;
        }

        return 0;
    }
}

#ifdef LX_DIST

#include <Windows.h>

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    return Lynx::Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
    return Lynx::Main(argc, argv);
}

#endif // LX_DIST

#endif // LX_PLATFORM_WINDOWS