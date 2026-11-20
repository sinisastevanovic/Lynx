#include "DeletionQueue.h"

namespace Lynx
{
    void DeletionQueue::Retire(std::function<void()> destroy)
    {
        m_Entries.push_back({ m_Frame, std::move(destroy) });
    }

    void DeletionQueue::BeginFrame()
    {
        ++m_Frame;

        // Entries are in the order they were retired, so the old ones are at the front.
        while (!m_Entries.empty() && m_Entries.front().RetiredInFrame + FramesToKeep <= m_Frame)
        {
            m_Entries.front().Destroy();
            m_Entries.pop_front();
        }
    }

    void DeletionQueue::Flush()
    {
        // Destroying an object may retire another one, so the queue is emptied until nothing is left.
        while (!m_Entries.empty())
        {
            std::function<void()> destroy = std::move(m_Entries.front().Destroy);
            m_Entries.pop_front();
            destroy();
        }
    }
}
