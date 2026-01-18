#include "DllLoader.h"

#if defined(_WIN32)
    #define NOMINMAX
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace Lynx
{
    DllHandle DllLoader::Load(const std::string& path)
    {
        DllHandle handle;
#if defined(_WIN32)
        handle.Ptr = LoadLibraryA(path.c_str());
#else
        handle.Ptr = dlopen(path.c_str(), RTLD_LAZY);
#endif
        if (!handle.IsValid()) { LX_CORE_ERROR("Failed to load library: {0}", path); }
        return handle;
    }

    void DllLoader::Unload(DllHandle handle)
    {
        if (!handle.IsValid()) return;
#if defined(_WIN32)
        FreeLibrary((HMODULE)handle.Ptr);
#else
        dlclose(handle.Ptr);
#endif
    }

    void* GetSymbolInternal(DllHandle handle, const std::string& name)
    {
        if (!handle.IsValid()) return nullptr;
#if defined(_WIN32)
        return (void*)GetProcAddress((HMODULE)handle.Ptr, name.c_str());
#else
        return dlsym(handle.Ptr, name.c_str());
#endif
    }
}
