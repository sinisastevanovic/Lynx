#include "FileWatcher.h"

namespace Lynx
{
    namespace
    {
        constexpr DWORD s_WatchedChanges = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME;
    }

    FileWatcher::FileWatcher(const std::filesystem::path& path, FileEventCallback callback)
        : m_Path(path), m_Callback(std::move(callback))
    {
        // Opened for overlapped I/O, so that the watcher thread can wait for a change and for the stop event at once.
        const HANDLE directory = CreateFileW(
            m_Path.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (directory == INVALID_HANDLE_VALUE)
        {
            LX_CORE_ERROR("FileWatcher: Failed to open directory {0}", m_Path.string());
            return;
        }

        m_DirectoryHandle = directory;
        m_StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        m_WatchThread = std::thread(&FileWatcher::WatchLoop, this);
    }

    FileWatcher::~FileWatcher()
    {
        if (!m_WatchThread.joinable())
            return;

        SetEvent(m_StopEvent);
        m_WatchThread.join();

        CloseHandle(m_StopEvent);
        CloseHandle(m_DirectoryHandle);
    }

    void FileWatcher::WatchLoop()
    {
        // ReadDirectoryChangesW needs a buffer aligned for DWORD.
        alignas(DWORD) char buffer[16 * 1024];

        OVERLAPPED overlapped = {};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        const HANDLE waitHandles[] = { overlapped.hEvent, m_StopEvent };

        // A rename arrives as two entries, the old name and then the new one.
        std::filesystem::path renamedFrom;

        while (true)
        {
            ResetEvent(overlapped.hEvent);
            if (!ReadDirectoryChangesW(m_DirectoryHandle, buffer, sizeof(buffer), TRUE, s_WatchedChanges, nullptr, &overlapped, nullptr))
            {
                LX_CORE_ERROR("FileWatcher: Stopped watching {0}, error {1}", m_Path.string(), GetLastError());
                break;
            }

            if (WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE) != WAIT_OBJECT_0)
            {
                // Asked to stop. The pending read still refers to buffer and overlapped,
                // so it is cancelled and waited for before they go out of scope.
                CancelIoEx(m_DirectoryHandle, &overlapped);
                DWORD ignored = 0;
                GetOverlappedResult(m_DirectoryHandle, &overlapped, &ignored, TRUE);
                break;
            }

            DWORD bytesWritten = 0;
            if (!GetOverlappedResult(m_DirectoryHandle, &overlapped, &bytesWritten, FALSE))
                continue;

            // Zero bytes means more changes happened than the buffer can hold. Those are lost.
            if (bytesWritten == 0)
            {
                LX_CORE_WARN("FileWatcher: Too many changes at once in {0}, some were missed", m_Path.string());
                continue;
            }

            ReportChanges(buffer, renamedFrom);
        }

        CloseHandle(overlapped.hEvent);
    }

    void FileWatcher::ReportChanges(const char* buffer, std::filesystem::path& renamedFrom)
    {
        const FILE_NOTIFY_INFORMATION* change = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer);
        while (true)
        {
            const std::wstring fileName(change->FileName, change->FileNameLength / sizeof(WCHAR));
            const std::filesystem::path fullPath = m_Path / fileName;

            switch (change->Action)
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
                    renamedFrom = fullPath;
                    break;
                case FILE_ACTION_RENAMED_NEW_NAME:
                    if (!renamedFrom.empty())
                    {
                        m_Callback(FileAction::Renamed, renamedFrom, fullPath);
                        renamedFrom.clear();
                    }
                    break;
            }

            if (change->NextEntryOffset == 0)
                break;

            change = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(reinterpret_cast<const char*>(change) + change->NextEntryOffset);
        }
    }
}
