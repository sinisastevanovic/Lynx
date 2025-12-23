#include "FileWatcher.h"

namespace Lynx
{
    FileWatcher::FileWatcher(const std::filesystem::path& path, FileEventCallback callback)
        : m_Path(path), m_Callback(callback), m_Running(true)
    {
        m_WatchThread = std::thread(&FileWatcher::WatchLoop, this);
    }

    FileWatcher::~FileWatcher()
    {
        m_Running = false;

        // TODO: Is this correct and safe??
        if (m_WatchThread.joinable())
        {
            m_WatchThread.detach();
        }
    }

    void FileWatcher::WatchLoop()
    {
        // Open a handle to the directory
        HANDLE hDir = CreateFileW(
            m_Path.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL
        );

        if (hDir == INVALID_HANDLE_VALUE)
        {
            LX_CORE_ERROR("FileWatcher: Failed to open directory {0}", m_Path.string());
            return;
        }

        char buffer[2048];
        DWORD bytesReturned;

        while (m_Running)
        {
            // This function BLOCKS until something happens
            if (ReadDirectoryChangesW(
                hDir,
                &buffer,
                sizeof(buffer),
                TRUE,
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
                &bytesReturned,
                NULL,
                NULL
            ))
            {
                // Iterate over the notifications in the buffer
                FILE_NOTIFY_INFORMATION* pNotify;
                int offset = 0;

                do
                {
                    pNotify = (FILE_NOTIFY_INFORMATION*)((char*)buffer + offset);

                    // Extract filename
                    std::wstring fileName(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));
                    std::filesystem::path fullPath = m_Path / fileName;
                    if (pNotify->Action == FILE_ACTION_MODIFIED ||
                        pNotify->Action == FILE_ACTION_ADDED ||
                        pNotify->Action == FILE_ACTION_RENAMED_NEW_NAME)
                    {
                        // Fire callback!
                        // Note: This runs on the background thread.
                        // Be careful accessing Main Thread stuff (like OpenGL/Vulkan) directly here!
                        // Usually you want to push this event to a queue.
                        m_Callback(fullPath);
                    }
                    offset += pNotify->NextEntryOffset;
                }
                while (pNotify->NextEntryOffset != 0);
            }
        }

        CloseHandle(hDir);
    }
}
