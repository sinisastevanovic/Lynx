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

        char buffer[4096];
        DWORD bytesReturned;

        std::filesystem::path oldRenamePath;

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

                    switch (pNotify->Action)
                    {
                        case FILE_ACTION_ADDED:
                            m_Callback(FileAction::Added, fullPath, "");
                            break;
                        case FILE_ACTION_REMOVED:
                            m_Callback(FileAction::Removed, fullPath, "");
                            break;
                        case FILE_ACTION_MODIFIED:
                            m_Callback(FileAction::Modified, fullPath, "");
                            break;
                        case FILE_ACTION_RENAMED_OLD_NAME:
                            oldRenamePath = fullPath;
                            break;
                        case FILE_ACTION_RENAMED_NEW_NAME:
                            if (!oldRenamePath.empty())
                            {
                                m_Callback(FileAction::Renamed, oldRenamePath, fullPath);
                                oldRenamePath.clear();
                            }
                            break;
                    }
                    
                    offset += pNotify->NextEntryOffset;
                }
                while (pNotify->NextEntryOffset != 0);
            }
        }

        CloseHandle(hDir);
    }
}
