#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Lynx
{
    // A fixed number of threads that run jobs in the order they were submitted.
    class WorkerPool
    {
    public:
        explicit WorkerPool(unsigned int threadCount);

        // Lets the jobs that are running finish, drops the ones still waiting, and joins the threads.
        // After it returns, no job is running, so whatever the jobs refer to may be destroyed.
        ~WorkerPool();

        WorkerPool(const WorkerPool&) = delete;
        WorkerPool& operator=(const WorkerPool&) = delete;

        void Submit(std::function<void()> job);

    private:
        void WorkerLoop();

        std::mutex m_Mutex;
        std::condition_variable m_JobAvailable;
        std::deque<std::function<void()>> m_Jobs;
        bool m_IsStopping = false;

        std::vector<std::thread> m_Threads;
    };
}
