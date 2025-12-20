#include "PhysicsSystem.h"

#include <cstdarg>
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

namespace Lynx
{
    // --- Jolt Configuration Classes ---

    // BroadPhase Layer Interface
    // This defines how Object Layers map to BroadPhase Layers.
    // Basically: "If I am a PLayer (MOVABLE), which generic bucket do I go in?"
    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
            m_ObjectToBroadPhase[Layers::STATIC] = BroadPhaseLayers::STATIC;
            m_ObjectToBroadPhase[Layers::MOVABLE] = BroadPhaseLayers::MOVABLE;
        }

        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }
        
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
            return m_ObjectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch ((JPH::BroadPhaseLayer::Type)inLayer)
            {
                case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::STATIC: return "STATIC";
                case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVABLE: return "MOVABLE";
                default: JPH_ASSERT(false); return "INVALID";
            }
        }
#endif

    private:
        JPH::BroadPhaseLayer m_ObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    // Object vs BroadPhase Layer Filter
    // This determines if a specific Object Layer should check for collision with a specific BroadPhase bucket.
    // Example: "Should a Player (MOVABLE) check collisions against Static Walls (STATIC)?" -> YES
    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            switch (inLayer1)
            {
                case Layers::STATIC:
                    // Static objects only collide with Moving objects
                    return inLayer2 == BroadPhaseLayers::MOVABLE;
                case Layers::MOVABLE:
                    // Moving objects collide with everything
                    return true;
                default:
                    JPH_ASSERT(false);
                    return false;
            }
        }
    };

    // Object vs Object Layer Filter
    // This is the fine-grained check. "Should a Player collide with an Enemy?"
    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
        {
            switch (inLayer1)
            {
                case Layers::STATIC:
                    // Static objects only collide with Moving objects
                    return inLayer2 == Layers::MOVABLE;
                case Layers::MOVABLE:
                    // Moving objects collide with everything
                    return true;
                default:
                    JPH_ASSERT(false);
                    return false;
            }
        }
    };

    // --- PhysicsSystem Implementation ---

    // Callback for traces
    static void TraceImpl(const char* inFMT, ...)
    {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);

        LX_CORE_TRACE("Jolt: {0}", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    // Callback for asserts
    static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
    {
        LX_ASSERT(false, "Jolt Assert Failed: {0} ({1}) at {2}:{3}", inExpression, (inMessage ? inMessage : ""), inFile, inLine);
        return true;
    };
#endif // JPH_ENABLE_ASSERTS

    PhysicsSystem::PhysicsSystem()
    {
        // TODO: maybe remove init and shutdown and move to constructor destructor..
        JPH::RegisterDefaultAllocator();

        JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
        JPH::AssertFailed = AssertFailedImpl;
#endif

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        m_BPLayerInterface = new BPLayerInterfaceImpl();
        m_ObjectVsBroadPhaseLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
        m_ObjectLayerPairFilter = new ObjectLayerPairFilterImpl();

        const JPH::uint cMaxBodies = 1024;
        const JPH::uint cNumBodyMutexes = 0;
        const JPH::uint cMaxBodyPairs = 1024;
        const JPH::uint cMaxContactConstraints = 1024;

        m_JoltPhysicsSystem.Init(
            cMaxBodies,
            cNumBodyMutexes,
            cMaxBodyPairs,
            cMaxContactConstraints,
            *m_BPLayerInterface,
            *m_ObjectVsBroadPhaseLayerFilter,
            *m_ObjectLayerPairFilter
        );

        m_TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
        // Job System (use all available threads -1)
        m_JobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
    }

    PhysicsSystem::~PhysicsSystem()
    {
        JPH::UnregisterTypes();

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;

        delete m_JobSystem;
        delete m_TempAllocator;

        delete m_BPLayerInterface;
        delete m_ObjectVsBroadPhaseLayerFilter;
        delete m_ObjectLayerPairFilter;

        m_JobSystem = nullptr;
        m_TempAllocator = nullptr;
        m_BPLayerInterface = nullptr;
        m_ObjectVsBroadPhaseLayerFilter = nullptr;
        m_ObjectLayerPairFilter = nullptr;
    }

    void PhysicsSystem::Simulate(float deltaTime)
    {
        // TODO:
        // If the framerate is too low, we need to clamp the delta time to avoid instability
        // For a real game, you might want a fixed timestep loop here.
        // For now, we simply step the physics world.

        // 1 collision step, 1 integration step (Keep it simple for Phase 1)
        // In the future, we will want to decouple physics time from render time.
        const int cCollisionSteps = 1;
        //const int cIntegrationSubSteps = 1; // TODO: What about this?

        m_JoltPhysicsSystem.Update(deltaTime, cCollisionSteps, m_TempAllocator, m_JobSystem);
    }
}
