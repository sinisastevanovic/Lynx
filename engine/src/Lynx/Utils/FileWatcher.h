#pragma once
#include <filesystem>

namespace Lynx
{
    enum class FileAction
    {
        Added,
        Removed,
        Modified,
        Renamed
    };
    
    class LX_API FileWatcher
    {
    public:
        using FileEventCallback = std::function<void(FileAction action, const std::filesystem::path& path, const std::filesystem::path& newPath)>;

        FileWatcher(const std::filesystem::path& path, FileEventCallback callback);
        ~FileWatcher();

    private:
        void WatchLoop();

    private:
        std::filesystem::path m_Path;
        FileEventCallback m_Callback;

        std::thread m_WatchThread;
        std::atomic<bool> m_Running;
    };
}
