#pragma once

#include <cstdint>
#include <deque>
#include <functional>

namespace Lynx
{
    // Destroys GPU objects a few frames after they were given up.
    //
    // A frame is recorded on the CPU while the GPU still draws earlier ones, so an object that the
    // renderer no longer wants may still be in use on the GPU. Destroying it at once is an error.
    // Retiring it here keeps it alive until every frame that could have used it is done.
    class DeletionQueue
    {
    public:
        // How many frames an object outlives its retirement. More than the number of frames
        // the CPU can be ahead of the GPU, which is two.
        static constexpr uint64_t FramesToKeep = 3;

        // The call that destroys the object. It runs after a few more frames have begun, or on Flush.
        void Retire(std::function<void()> destroy);

        // Call at the start of every frame. Destroys what was retired long enough ago.
        void BeginFrame();

        // Destroys everything at once. The GPU has to be idle.
        void Flush();

    private:
        struct Entry
        {
            uint64_t RetiredInFrame = 0;
            std::function<void()> Destroy;
        };

        std::deque<Entry> m_Entries;
        uint64_t m_Frame = 0;
    };
}
