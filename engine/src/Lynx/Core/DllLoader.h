#pragma once

namespace Lynx
{
    struct DllHandle
    {
        void* Ptr = nullptr;
        bool IsValid() const
        {
            return Ptr != nullptr;
        }
    };
    
    class LX_API DllLoader
    {
    public:
        // Turns a module name into the file name of a shared library on this platform, e.g. "game" into "game.dll".
        static std::string PlatformFileName(const std::string& moduleName);

        static DllHandle Load(const std::string& path);
        static void Unload(DllHandle handle);
        
        template<typename FuncType>
        static FuncType GetSymbol(DllHandle handle, const std::string& name);
    };
    
    LX_API void* GetSymbolInternal(DllHandle handle, const std::string& name);
    
    template<typename FuncType>
    FuncType DllLoader::GetSymbol(DllHandle handle, const std::string& name)
    {
        return (FuncType)GetSymbolInternal(handle, name);
    }
}

