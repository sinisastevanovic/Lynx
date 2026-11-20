#include "WorkerPool.h"

namespace Lynx
{
    WorkerPool::WorkerPool(unsigned int threadCount)
    {
        m_Threads.reserve(threadCount);
        for (unsigned int i = 0; i < threadCount; ++i)
            m_Threads.emplace_back(&WorkerPool::WorkerLoop, this);
    }

    WorkerPool::~WorkerPool()
    {
        {
            std::lock_guard lock(m_Mutex);
            m_IsStopping = true;
            m_Jobs.clear();
        }
        m_JobAvailable.notify_all();

        for (std::thread& thread : m_Threads)
            thread.join();
    }

    void WorkerPool::Submit(std::function<void()> job)
    {
        {
            std::lock_guard lock(m_Mutex);
            m_Jobs.push_back(std::move(job));
        }
        m_JobAvailable.notify_one();
    }

    void WorkerPool::WorkerLoop()
    {
        while (true)
        {
            std::function<void()> job;
            {
                std::unique_lock lock(m_Mutex);
                m_JobAvailable.wait(lock, [this] { return m_IsStopping || !m_Jobs.empty(); });
                if (m_IsStopping)
                    return;

                job = std::move(m_Jobs.front());
                m_Jobs.pop_front();
            }

            job();
        }
    }
}
