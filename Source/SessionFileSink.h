#pragma once
#include <spdlog/details/file_helper.h>
#include <spdlog/sinks/base_sink.h>

// TODO: Implement memory mapped file

namespace Lynx
{
    using namespace spdlog;
    using namespace spdlog::sinks;
    
    /**
     * Rotating file sink where a new log gets created on every start.
     * Old ones get renamed with DateTime appended to them.
     */
    class SessionFileSink final : public base_sink<std::mutex>
    {
    public:
        SessionFileSink(filename_t baseFileName, std::size_t maxFiles, const file_event_handlers &event_handlers = {});

        virtual ~SessionFileSink() = default;
        SessionFileSink(const SessionFileSink& other) = delete;
        SessionFileSink& operator=(const SessionFileSink& other) = delete;

        filename_t fileName();
        
    protected:
        virtual void sink_it_(const details::log_msg& msg) override;
        virtual void flush_() override;

    private:
        void RotateOldFile();
        
        filename_t m_baseFileName;
        std::size_t m_maxFiles;
        details::file_helper m_fileHelper;
    };

}
