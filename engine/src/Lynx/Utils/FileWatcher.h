#pragma once

#include "Lynx/Core.h"

#include <filesystem>
#include <functional>
#include <thread>

namespace Lynx
{
    enum class FileAction
    {
        Added,
        Removed,
        Modified,
        Renamed
    };

    // Reports changes to the files below a directory, including its subdirectories.
    class LX_API FileWatcher
    {
    public:
        // Called on the watcher's own thread, never on the thread that created the watcher.
        using FileEventCallback = std::function<void(FileAction action, const std::filesystem::path& path, const std::filesystem::path& newPath)>;

        FileWatcher(const std::filesystem::path& path, FileEventCallback callback);

        // Stops watching and waits for the watcher thread. The callback is not called after this returns.
        ~FileWatcher();

        FileWatcher(const FileWatcher&) = delete;
        FileWatcher& operator=(const FileWatcher&) = delete;

    private:
        void WatchLoop();
        void ReportChanges(const char* buffer, std::filesystem::path& renamedFrom);

    private:
        std::filesystem::path m_Path;
        FileEventCallback m_Callback;

        // Win32 handles, kept as void* so that this header does not need Windows.h.
        void* m_DirectoryHandle = nullptr;
        void* m_StopEvent = nullptr;

        std::thread m_WatchThread;
    };
}
