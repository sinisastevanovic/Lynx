#include "lxpch.h"
#include "SessionFileSink.h"

#include <filesystem>

namespace Lynx
{
    using namespace spdlog;
    using namespace spdlog::sinks;

    static bool RenameFile(const filename_t& srcFileName, const filename_t& targetFileName)
    {
        (void)details::os::remove(targetFileName);
        return details::os::rename(srcFileName, targetFileName) == 0;
    }

    template <typename TP>
    static time_t ToTimeT(TP tp)
    {
        using namespace std::chrono;
        auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
        return system_clock::to_time_t(sctp);
    }

    SessionFileSink::SessionFileSink(filename_t baseFileName, std::size_t maxFiles, const file_event_handlers &event_handlers)
        : m_baseFileName(baseFileName), m_maxFiles(maxFiles), m_fileHelper{event_handlers}
    {
        RotateOldFile();
        m_fileHelper.open(baseFileName);
    }

    filename_t SessionFileSink::fileName()
    {
        std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
        return m_fileHelper.filename();
    }

    void SessionFileSink::sink_it_(const details::log_msg& msg)
    {
        memory_buf_t formatted;
        base_sink<std::mutex>::formatter_->format(msg, formatted);
        m_fileHelper.write(formatted);
    }

    void SessionFileSink::flush_()
    {
        m_fileHelper.flush();
    }

    void SessionFileSink::RotateOldFile()
    {
        using details::os::filename_to_str;
        using details::os::path_exists;

        // If file already exists, rename
        if (path_exists(m_baseFileName))
        {
            std::filesystem::directory_entry fsFile{m_baseFileName};
            auto tm = details::os::localtime(ToTimeT(fsFile.last_write_time()));
            filename_t basename, ext;
            std::tie(basename, ext) = details::file_helper::split_by_extension(m_baseFileName);
            filename_t targetFileName = fmt_lib::format(SPDLOG_FMT_STRING(SPDLOG_FILENAME_T("{}_{:04d}-{:02d}-{:02d}-{:02d}-{:02d}-{:02d}{}")),
                basename, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ext);
            if (!RenameFile(m_baseFileName, targetFileName))
            {
                // if failed try again after a small delay.
                // this is a workaround to a windows issue, where very high rotation
                // rates can cause the rename to fail with permission denied (because of antivirus?).
                details::os::sleep_for_millis(100);
                if (!RenameFile(m_baseFileName, targetFileName))
                {
                    // TODO: Throw exception!
                }
            }
        }

        // Delete oldest files if there are more than m_maxFiles
        if (m_maxFiles > 0)
        {
            std::filesystem::path fsPath{m_baseFileName};
            uint32_t numFiles = 0;
            std::set<std::filesystem::path> sortedByName;
            for (const auto& entry : std::filesystem::directory_iterator(fsPath.parent_path()))
            {
                if (entry.is_regular_file())
                {
                    numFiles++;
                    sortedByName.insert(entry.path());
                }
            }

            if (numFiles >= m_maxFiles)
            {
                for (auto& fileName : sortedByName)
                {
                    if (details::os::remove(fileName.string()) == 0)
                    {
                        numFiles--;
                        if (numFiles < m_maxFiles)
                            break;
                    }
                }
            }
        }
    }

    
}
