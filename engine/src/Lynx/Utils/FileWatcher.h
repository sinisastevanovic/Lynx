#pragma once
#include <filesystem>

namespace Lynx
{
    class LX_API FileWatcher
    {
    public:
        using FileEventCallback = std::function<void(const std::filesystem::path&)>;

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
