#include "PhysicsWorld.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <unordered_map>
#include <mutex>
#include <Jolt/Physics/Character/CharacterBase.h>

namespace
{
    inline JPH::Vec3 ToJolt(const glm::vec3& v) { return {v.x, v.y, v.z}; }
    inline JPH::Quat ToJolt(const glm::quat& q) { return {q.x, q.y, q.z, q.w}; }
    inline glm::vec3 ToGLM(const JPH::Vec3& v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
    inline glm::quat ToGLM(const JPH::Quat& q) { return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()}; }
    
    inline JPH::EMotionType ToJolt(Lynx::BodyType type)
    {
        switch (type)
        {
            case Lynx::BodyType::Static: return JPH::EMotionType::Static;
            case Lynx::BodyType::Dynamic: return JPH::EMotionType::Dynamic;
            case Lynx::BodyType::Kinematic: return JPH::EMotionType::Kinematic;
            default: return JPH::EMotionType::Static;
        }
    }
    
    inline Lynx::GroundState FromJolt(JPH::CharacterVirtual::EGroundState state)
    {
        switch (state)
        {
            case JPH::CharacterBase::EGroundState::OnGround:
                return Lynx::GroundState::Grounded;
            case JPH::CharacterBase::EGroundState::OnSteepGround:
                return Lynx::GroundState::OnSteepGround;
            case JPH::CharacterBase::EGroundState::NotSupported:
                return Lynx::GroundState::NotSupported;
            case JPH::CharacterBase::EGroundState::InAir:
                return Lynx::GroundState::InAir;
        }
        return Lynx::GroundState::InAir;
    }

    namespace BroadPhaseLayers 
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr uint32_t NUM_LAYERS = 2;
    }
    
    class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
        {
            static const JPH::BroadPhaseLayer mapping[] = {
                BroadPhaseLayers::NON_MOVING,
                BroadPhaseLayers::MOVING,
            };
            return mapping[layer < 2 ? layer : 1];
        }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
            switch ((JPH::BroadPhaseLayer::Type)layer) {
                case 0: return "NON_MOVING";
                case 1: return "MOVING";
                default: return "UNKNOWN";
            }
        }
#endif
    };
    
    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer object, JPH::BroadPhaseLayer broadphase) const override {
            // Static objects don't collide with static broadphase
            if (object == Lynx::CollisionLayer::Static)
                return broadphase != BroadPhaseLayers::NON_MOVING;
            return true;
        }
    };

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
            // Static vs Static: never
            if (a == Lynx::CollisionLayer::Static && b == Lynx::CollisionLayer::Static)
                return false;
            
            // Triggers only collide with Player and Dynamic
            if (a == Lynx::CollisionLayer::Trigger || b == Lynx::CollisionLayer::Trigger) {
                auto other = (a == Lynx::CollisionLayer::Trigger) ? b : a;
                return other == Lynx::CollisionLayer::Player || other == Lynx::CollisionLayer::Character || other == Lynx::CollisionLayer::Dynamic;
            }
            
            return true;
        }
    };
    
    class LayerMaskObjectFilter : public JPH::ObjectLayerFilter
    {
    public:
        explicit LayerMaskObjectFilter(uint8_t mask) : m_Mask(mask) {}
    
        bool ShouldCollide(JPH::ObjectLayer inLayer) const override
        {
            return (m_Mask & (1 << inLayer)) != 0;
        }
    
    private:
        uint8_t m_Mask;
    };
}

namespace Lynx
{
    // --- Contact Listener ---
    class ContactListenerImpl : public JPH::ContactListener
    {
    public:
        std::mutex Mutex;
        std::vector<CollisionEvent> EnterEvents;
        std::vector<CollisionEvent> StayEvents;
        std::vector<CollisionEvent> ExitEvents;
        
        void Clear()
        {
            std::lock_guard lock(Mutex);
            EnterEvents.clear();
            StayEvents.clear();
            ExitEvents.clear();
        }
        
        JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override
        {
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }
        
        void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override
        {
            CollisionEvent event;
            event.BodyA = inBody1.GetID().GetIndexAndSequenceNumber();
            event.BodyB = inBody2.GetID().GetIndexAndSequenceNumber();
            event.EntityA = static_cast<entt::entity>(inBody1.GetUserData());
            event.EntityB = static_cast<entt::entity>(inBody2.GetUserData());
            event.IsTrigger = inBody1.IsSensor() || inBody2.IsSensor();
            
            if (inManifold.mRelativeContactPointsOn1.size() > 0)
                event.ContactPoint = ToGLM(inManifold.mBaseOffset + inManifold.mRelativeContactPointsOn1[0]);
            
            event.ContactNormal = ToGLM(inManifold.mWorldSpaceNormal);
            event.PenetrationDepth = inManifold.mPenetrationDepth;
            
            std::lock_guard lock(Mutex);
            EnterEvents.push_back(event);
        }
        
        void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override
        {
            CollisionEvent event;
            event.BodyA = inBody1.GetID().GetIndexAndSequenceNumber();
            event.BodyB = inBody2.GetID().GetIndexAndSequenceNumber();
            uint64_t userData = inBody1.GetUserData();
            event.EntityA = static_cast<entt::entity>(inBody1.GetUserData());
            event.EntityB = static_cast<entt::entity>(inBody2.GetUserData());
            event.IsTrigger = inBody1.IsSensor() || inBody2.IsSensor();
            
            if (inManifold.mRelativeContactPointsOn1.size() > 0)
                event.ContactPoint = ToGLM(inManifold.mBaseOffset + inManifold.mRelativeContactPointsOn1[0]);
            
            event.ContactNormal = ToGLM(inManifold.mWorldSpaceNormal);
            event.PenetrationDepth = inManifold.mPenetrationDepth;
            
            std::lock_guard lock(Mutex);
            StayEvents.push_back(event);
        }
        
        void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
        {
            CollisionEvent event;
            event.BodyA = inSubShapePair.GetBody1ID().GetIndexAndSequenceNumber();
            event.BodyB = inSubShapePair.GetBody2ID().GetIndexAndSequenceNumber();
            
            // TODO: Get entities here...
            //event.EntityA = 
            //event.EntityB =
            
            std::lock_guard lock(Mutex);
            ExitEvents.push_back(event);
        }
    };
    
    // --- Character Contact Listener ---
    class CharacterContactListenerImpl : public JPH::CharacterContactListener {
    public:
        std::mutex Mutex;
        std::vector<CharacterCollision> Collisions;

        void clear() {
            std::lock_guard lock(Mutex);
            Collisions.clear();
        }

        void OnContactAdded(
            const JPH::CharacterVirtual* character,
            const JPH::BodyID& bodyID2,
            const JPH::SubShapeID& subShapeID2,
            JPH::RVec3Arg contactPosition,
            JPH::Vec3Arg contactNormal,
            JPH::CharacterContactSettings& settings
        ) override {
            CharacterCollision col;
            col.CharacterA = character->GetID().GetValue();
            col.CharacterEntity = static_cast<entt::entity>(character->GetUserData());
            col.BodyA = bodyID2.GetIndexAndSequenceNumber();
            //col.OtherEntity = TODO:!!
            col.ContactPoint = ToGLM(contactPosition);
            col.ContactNormal = ToGLM(contactNormal);

            std::lock_guard lock(Mutex);
            Collisions.push_back(col);
        }

        void OnContactSolve(
            const JPH::CharacterVirtual* character,
            const JPH::BodyID& bodyID2,
            const JPH::SubShapeID& subShapeID2,
            JPH::RVec3Arg contactPosition,
            JPH::Vec3Arg contactNormal,
            JPH::Vec3Arg contactVelocity,
            const JPH::PhysicsMaterial* contactMaterial,
            JPH::Vec3Arg characterVelocity,
            JPH::Vec3& newCharacterVelocity
        ) override {
            // Default behavior: let Jolt handle it
        }
    };
    
    // --- Pimpl Implementation ---
    class JoltPhysicsImpl
    {
    public:
        // Jolt Systems
        std::unique_ptr<JPH::TempAllocatorImpl> TempAllocator;
        std::unique_ptr<JPH::JobSystemThreadPool> JobSystem;
        std::unique_ptr<JPH::PhysicsSystem> PhysicsSystem;
        
        // Layer Interfaces
        BroadPhaseLayerInterfaceImpl BroadPhaseLayerInterface;
        ObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadPhaseLayerFilter;
        ObjectLayerPairFilterImpl ObjectLayerPairFilter;
        
        // Listeners
        ContactListenerImpl ContactListener;
        CharacterContactListenerImpl CharacterContactListener;
        
        // Character Mangement
        struct CharacterData
        {
            std::unique_ptr<JPH::CharacterVirtual> Character;
            entt::entity Entity = entt::null;
        };
        
        std::unordered_map<CharacterId, CharacterData> Characters;
        CharacterId NextCharacterId = 0;
        
        // Cached Event Buffers
        std::vector<CollisionEvent> CollisionEnterEvents;
        std::vector<CollisionEvent> CollisionStayEvents;
        std::vector<CollisionEvent> CollisionExitEvents;
        std::vector<CharacterCollision> CharacterCollisions;
        
        glm::vec3 Gravity{0.0f, -9.81f, 0.0f};
        
        JoltPhysicsImpl()
        {
            // Intialize Jolt
            JPH::RegisterDefaultAllocator();
            if (!JPH::Factory::sInstance)
                JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();

            // Allocators
            TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
            JobSystem = std::make_unique<JPH::JobSystemThreadPool>(
                JPH::cMaxPhysicsJobs, 
                JPH::cMaxPhysicsBarriers, 
                static_cast<int>(std::thread::hardware_concurrency()) - 1
            );
            
            const uint32_t maxBodies = 65536;
            const uint32_t numBodyMutexes = 0; // Auto
            const uint32_t maxBodyPairs = 65536;
            const uint32_t maxContactConstraints = 10240;
            
            PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
            PhysicsSystem->Init(
                maxBodies,
                numBodyMutexes,
                maxBodyPairs,
                maxContactConstraints,
                BroadPhaseLayerInterface,
                ObjectVsBroadPhaseLayerFilter,
                ObjectLayerPairFilter
            );
            
            PhysicsSystem->SetContactListener(&ContactListener);
            PhysicsSystem->SetGravity(ToJolt(Gravity));
        }
        
        ~JoltPhysicsImpl()
        {
            Characters.clear();
            PhysicsSystem.reset();
            JobSystem.reset();
            TempAllocator.reset();
        }
    };


    PhysicsWorld::PhysicsWorld() : m_Impl(std::make_unique<JoltPhysicsImpl>()) {}

    PhysicsWorld::~PhysicsWorld()
    {
        if (!m_Impl || !m_Impl->PhysicsSystem)
            return;
        
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        JPH::BodyIDVector bodyIds;
        m_Impl->PhysicsSystem->GetBodies(bodyIds);
        
        for (const auto& id : bodyIds)
        {
            bodyInterface.RemoveBody(id);
            bodyInterface.DestroyBody(id);
        }
        
        m_Impl.reset();
    }

    void PhysicsWorld::Step(float deltaTime)
    {
        // Clear previous frame events
        m_Impl->CollisionEnterEvents.clear();
        m_Impl->CollisionStayEvents.clear();
        m_Impl->CollisionExitEvents.clear();
        m_Impl->CharacterCollisions.clear();
        
        // Step physics
        const int collisionSteps = 1;
        m_Impl->PhysicsSystem->Update(deltaTime, collisionSteps, m_Impl->TempAllocator.get(), m_Impl->JobSystem.get());
        
        // Collect Events
        {
            std::lock_guard lock(m_Impl->ContactListener.Mutex);
            m_Impl->CollisionEnterEvents = std::move(m_Impl->ContactListener.EnterEvents);
            m_Impl->CollisionStayEvents = std::move(m_Impl->ContactListener.StayEvents);
            m_Impl->CollisionExitEvents = std::move(m_Impl->ContactListener.ExitEvents);
        }
        {
            std::lock_guard lock(m_Impl->CharacterContactListener.Mutex);
            m_Impl->CharacterCollisions = std::move(m_Impl->CharacterContactListener.Collisions);
        }
    }

    BodyId PhysicsWorld::CreateBody(const BodyDescriptor& desc)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        
        // Create shape
        JPH::ShapeRefC shape = std::visit([](auto&& s) -> JPH::ShapeRefC
        {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, BoxShape>) {
                return new JPH::BoxShape(ToJolt(s.HalfExtents));
            } else if constexpr (std::is_same_v<T, SphereShape>) {
                return new JPH::SphereShape(s.Radius);
            } else if constexpr (std::is_same_v<T, CapsuleShape>) {
                return new JPH::CapsuleShape(s.HalfHeight, s.Radius);
            } else if constexpr (std::is_same_v<T, CylinderShape>) {
                return new JPH::CylinderShape(s.HalfHeight, s.Radius);
            } else if constexpr (std::is_same_v<T, MeshShape>) {
                /*JPH::TriangleList triangles;
                for (size_t i = 0; i + 2 < s.Indices.size(); i += 3)
                {
                    triangles.push_back(JPH::Triangle(
                        toJolt(s.Vertices[s.Indices[i]]),
                        toJolt(s.Vertices[s.Indices[i + 1]]),
                        toJolt(s.Vertices[s.Indices[i + 2]])
                    ));
                }*/
                //return new JPH::MeshShape(triangles); // TODO:??
            }
            return new JPH::SphereShape(0.5f);
        }, desc.Shape);
        
        // Determine object layer
        JPH::ObjectLayer objectLayer = desc.Layer;
        JPH::BodyCreationSettings settings(
            shape,
            ToJolt(desc.Position),
            ToJolt(desc.Rotation),
            ToJolt(desc.Type),
            objectLayer
        );
        settings.mFriction = desc.Friction;
        settings.mRestitution = desc.Restitution;
        settings.mLinearDamping = desc.LinearDamping;
        settings.mAngularDamping = desc.AngularDamping;
        settings.mGravityFactor = desc.GravityFactor;
        settings.mIsSensor = desc.IsTrigger;
        settings.mUserData = static_cast<uint64_t>(entt::to_integral(desc.Entity));
        settings.mMotionQuality = (desc.MotionQuality == MotionQuality::Continuous) ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
        if (desc.Type == BodyType::Dynamic)
        {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = desc.Mass;
        }
        
        JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
        return bodyId.GetIndexAndSequenceNumber();
    }

    void PhysicsWorld::DestroyBody(BodyId id)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        JPH::BodyID joltId(id);
        bodyInterface.RemoveBody(joltId);
        bodyInterface.DestroyBody(joltId); // TODO: Is the way we handle ids correct??
    }

    bool PhysicsWorld::IsValidBody(BodyId id) const
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        return bodyInterface.IsAdded(JPH::BodyID(id));
    }

    glm::vec3 PhysicsWorld::GetPosition(BodyId id) const
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        return ToGLM(bodyInterface.GetPosition(JPH::BodyID(id)));
    }

    glm::quat PhysicsWorld::GetRotation(BodyId id) const
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        return ToGLM(bodyInterface.GetRotation(JPH::BodyID(id)));
    }

    glm::vec3 PhysicsWorld::GetLinearVelocity(BodyId id) const
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        return ToGLM(bodyInterface.GetLinearVelocity(JPH::BodyID(id)));
    }

    glm::vec3 PhysicsWorld::GetAngularVelocity(BodyId id) const
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        return ToGLM(bodyInterface.GetAngularVelocity(JPH::BodyID(id)));
    }

    glm::vec3 PhysicsWorld::GetCenterOfMass(BodyId id) const
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        return ToGLM(bodyInterface.GetCenterOfMassPosition(JPH::BodyID(id)));
    }

    entt::entity PhysicsWorld::GetEntity(BodyId id) const
    {
        // TODO: Why do we need this body lock??
        auto& bodyLockInterface = m_Impl->PhysicsSystem->GetBodyLockInterface();
        JPH::BodyLockRead lock(bodyLockInterface, JPH::BodyID(id));
        if (lock.Succeeded()) {
            uint64_t userData = lock.GetBody().GetUserData();
            return static_cast<entt::entity>(userData);
        }
        return entt::null;
    }

    bool PhysicsWorld::IsActive(BodyId id) const
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        return bodyInterface.IsActive(JPH::BodyID(id));
    }

    void PhysicsWorld::SetPosition(BodyId id, const glm::vec3& position, bool activate)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.SetPosition(JPH::BodyID(id), ToJolt(position), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    }

    void PhysicsWorld::SetRotation(BodyId id, const glm::quat& rotation, bool activate)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.SetRotation(JPH::BodyID(id), ToJolt(rotation), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    }

    void PhysicsWorld::SetPositionAndRotation(BodyId id, const glm::vec3& position, const glm::quat& rotation, bool activate)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.SetPositionAndRotation(
            JPH::BodyID(id),
            ToJolt(position),
            ToJolt(rotation),
            activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate
        );
    }

    void PhysicsWorld::SetLinearVelocity(BodyId id, const glm::vec3& velocity)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.SetLinearVelocity(JPH::BodyID(id), ToJolt(velocity));
    }

    void PhysicsWorld::SetAngularVelocity(BodyId id, const glm::vec3& velocity)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.SetAngularVelocity(JPH::BodyID(id), ToJolt(velocity));
    }

    void PhysicsWorld::SetGravityFactor(BodyId id, float factor)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.SetGravityFactor(JPH::BodyID(id), factor);
    }

    void PhysicsWorld::MoveKinematic(BodyId id, const glm::vec3& targetPos, const glm::quat& targetRot, float deltaTime)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.MoveKinematic(JPH::BodyID(id), ToJolt(targetPos), ToJolt(targetRot), deltaTime);
    }

    void PhysicsWorld::AddForce(BodyId id, const glm::vec3& force)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.AddForce(JPH::BodyID(id), ToJolt(force));
    }

    void PhysicsWorld::AddForceAtPoint(BodyId id, const glm::vec3& force, const glm::vec3& point)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.AddForce(JPH::BodyID(id), ToJolt(force), ToJolt(point));
    }

    void PhysicsWorld::AddImpulse(BodyId id, const glm::vec3& impulse)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.AddImpulse(JPH::BodyID(id), ToJolt(impulse));
    }

    void PhysicsWorld::AddImpulseAtPoint(BodyId id, const glm::vec3& impulse, const glm::vec3& point)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.AddImpulse(JPH::BodyID(id), ToJolt(impulse), ToJolt(point));
    }

    void PhysicsWorld::AddTorque(BodyId id, const glm::vec3& torque)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.AddTorque(JPH::BodyID(id), ToJolt(torque));
    }

    void PhysicsWorld::AddAngularImpulse(BodyId id, const glm::vec3& impulse)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.AddAngularImpulse(JPH::BodyID(id), ToJolt(impulse));
    }

    void PhysicsWorld::Activate(BodyId id)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.ActivateBody(JPH::BodyID(id));
    }

    void PhysicsWorld::Deactivate(BodyId id)
    {
        auto& bodyInterface = m_Impl->PhysicsSystem->GetBodyInterface();
        bodyInterface.DeactivateBody(JPH::BodyID(id));
    }

    CharacterId PhysicsWorld::CreateCharacter(const CharacterDescriptor& desc)
    {
        JPH::CharacterVirtualSettings settings;
        settings.mShape = new JPH::CapsuleShape(desc.CapsuleHalfHeight, desc.CapsuleRadius);
        settings.mMass = desc.Mass;
        settings.mMaxSlopeAngle = desc.MaxSlopeAngle;
        settings.mMaxStrength = desc.MaxStrength;
        settings.mPredictiveContactDistance = desc.PredicitveContactDistance;
        settings.mPenetrationRecoverySpeed = desc.PenetrationRecoverySpeed;
        
        auto character = std::make_unique<JPH::CharacterVirtual>(
            &settings,
            ToJolt(desc.Position),
            ToJolt(desc.Rotation),
            m_Impl->PhysicsSystem.get()
        );
        
        character->SetListener(&m_Impl->CharacterContactListener);
        
        CharacterId id = m_Impl->NextCharacterId++;
        uint64_t userData = static_cast<uint64_t>(entt::to_integral(desc.Entity));
        character->SetUserData(userData);
        m_Impl->Characters[id] = { std::move(character), desc.Entity };
        return id;
    }

    void PhysicsWorld::DestroyCharacter(CharacterId id)
    {
        m_Impl->Characters.erase(id); // TODO: Does this destroy it???
    }

    bool PhysicsWorld::IsValidCharacter(CharacterId id) const
    {
        return m_Impl->Characters.contains(id);
    }

    glm::vec3 PhysicsWorld::GetCharacterPosition(CharacterId id) const
    {
        auto it = m_Impl->Characters.find(id);
        if (it != m_Impl->Characters.end()) {
            return ToGLM(it->second.Character->GetPosition());
        }
        return glm::vec3(0.0f);
    }

    glm::quat PhysicsWorld::GetCharacterRotation(CharacterId id) const
    {
        auto it = m_Impl->Characters.find(id);
        if (it != m_Impl->Characters.end()) {
            return ToGLM(it->second.Character->GetRotation());
        }
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    glm::vec3 PhysicsWorld::GetCharacterLinearVelocity(CharacterId id) const
    {
        auto it = m_Impl->Characters.find(id);
        if (it != m_Impl->Characters.end()) {
            return ToGLM(it->second.Character->GetLinearVelocity());
        }
        return glm::vec3(0.0f);
    }

    glm::vec3 PhysicsWorld::GetCharacterGroundVelocity(CharacterId id) const
    {
        auto it = m_Impl->Characters.find(id);
        if (it != m_Impl->Characters.end()) {
            return ToGLM(it->second.Character->GetGroundVelocity());
        }
        return glm::vec3(0.0f);
    }

    glm::vec3 PhysicsWorld::GetCharacterGroundNormal(CharacterId id) const
    {
        auto it = m_Impl->Characters.find(id);
        if (it != m_Impl->Characters.end()) {
            return ToGLM(it->second.Character->GetGroundNormal());
        }
        return glm::vec3(0.0f, 1.0f, 0.0f);
    }

    GroundState PhysicsWorld::GetCharacterGroundState(CharacterId id) const
    {
        auto it = m_Impl->Characters.find(id);
        if (it != m_Impl->Characters.end()) {
            return FromJolt(it->second.Character->GetGroundState());
        }
        return GroundState::InAir;
    }

    bool PhysicsWorld::IsCharacterGrounded(CharacterId id) const
    {
        GroundState groundState = GetCharacterGroundState(id);
        return groundState == GroundState::Grounded;
    }

    entt::entity PhysicsWorld::GetCharacterEntity(CharacterId id) const
    {
        auto it = m_Impl->Characters.find(id);
        if (it != m_Impl->Characters.end()) {
            return it->second.Entity;
        }
        return entt::null;
    }

    void PhysicsWorld::SetCharacterPosition(CharacterId id, const glm::vec3& position)
    {
        auto it = m_Impl->Characters.find(id);
        if (it != m_Impl->Characters.end()) {
            it->second.Character->SetPosition(ToJolt(position));
        }
    }

    void PhysicsWorld::SetCharacterRotation(CharacterId id, const glm::quat& rotation)
    {
        auto it = m_Impl->Characters.find(id);
        if (it != m_Impl->Characters.end()) {
            it->second.Character->SetRotation(ToJolt(rotation));
        }
    }

    void PhysicsWorld::SetCharacterLinearVelocity(CharacterId id, const glm::vec3& velocity)
    {
        auto it = m_Impl->Characters.find(id);
        if (it != m_Impl->Characters.end()) {
            it->second.Character->SetLinearVelocity(ToJolt(velocity));
        }
    }

    void PhysicsWorld::UpdateCharacter(CharacterId id, float deltaTime, const glm::vec3& desiredVelocity)
    {
        auto it = m_Impl->Characters.find(id);
        if (it == m_Impl->Characters.end()) 
            return;
        
        auto* character = it->second.Character.get();
        auto& physics = *m_Impl->PhysicsSystem;
        
        character->SetLinearVelocity(ToJolt(desiredVelocity));
        
        JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
        
        JPH::BroadPhaseLayerFilter broadPhaseLayerFilter;
        JPH::ObjectLayerFilter objectLayerFilter;
        JPH::BodyFilter bodyFilter;
        JPH::ShapeFilter shapeFilter;
        
        character->ExtendedUpdate(
            deltaTime,
            ToJolt(m_Impl->Gravity),
            updateSettings,
            broadPhaseLayerFilter,
            objectLayerFilter,
            bodyFilter,
            shapeFilter,
            *m_Impl->TempAllocator
        );
    }

    RaycastHit PhysicsWorld::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, uint8_t layerMask) const
    {
        JPH::RRayCast ray(ToJolt(origin), ToJolt(direction) * maxDistance);
        JPH::RayCastResult result;
        
        JPH::BroadPhaseLayerFilter broadPhaseLayerFilter;
        LayerMaskObjectFilter objectFilter(layerMask);
        
        if (m_Impl->PhysicsSystem->GetNarrowPhaseQuery().CastRay(ray, result, broadPhaseLayerFilter, objectFilter))
        {
            RaycastHit hit;
            hit.Hit = true;
            hit.BodyId = result.mBodyID.GetIndexAndSequenceNumber();
            hit.Distance = result.mFraction * maxDistance;
            hit.Point = origin + direction * hit.Distance;
            
            auto& bodyLockInterface = m_Impl->PhysicsSystem->GetBodyLockInterface();
            JPH::BodyLockRead lock(bodyLockInterface, result.mBodyID);
            if (lock.Succeeded())
            {
                hit.Normal = ToGLM(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction)));
                hit.Entity = static_cast<entt::entity>(lock.GetBody().GetUserData());
            }
            return hit;
        }
        
        return {};
    }

    std::vector<RaycastHit> PhysicsWorld::RaycastAll(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, uint8_t layerMask) const
    {
        std::vector<RaycastHit> hits;

        JPH::RRayCast ray(ToJolt(origin), ToJolt(direction) * maxDistance);

        class Collector : public JPH::CastRayCollector {
        public:
            std::vector<JPH::RayCastResult> results;

            void AddHit(const JPH::RayCastResult& result) override {
                results.push_back(result);
            }
        } collector;
        
        JPH::BroadPhaseLayerFilter broadPhaseLayerFilter;
        LayerMaskObjectFilter objectFilter(layerMask);

        m_Impl->PhysicsSystem->GetNarrowPhaseQuery().CastRay(ray, {}, collector, broadPhaseLayerFilter, objectFilter);

        auto& bodyLockInterface = m_Impl->PhysicsSystem->GetBodyLockInterface();
        for (const auto& result : collector.results) {
            RaycastHit hit;
            hit.Hit = true;
            hit.BodyId = result.mBodyID.GetIndexAndSequenceNumber();
            hit.Distance = result.mFraction * maxDistance;
            hit.Point = origin + direction * hit.Distance;

            JPH::BodyLockRead lock(bodyLockInterface, result.mBodyID);
            if (lock.Succeeded()) {
                hit.Normal = ToGLM(
                    lock.GetBody().GetWorldSpaceSurfaceNormal(
                        result.mSubShapeID2,
                        ray.GetPointOnRay(result.mFraction)
                    )
                );
                hit.Entity = static_cast<entt::entity>(lock.GetBody().GetUserData());
            }

            hits.push_back(hit);
        }

        // Sort by distance
        std::sort(hits.begin(), hits.end(),
            [](const RaycastHit& a, const RaycastHit& b) { return a.Distance < b.Distance; });

        return hits;
    }

    std::vector<BodyId> PhysicsWorld::OverlapSphere(const glm::vec3& center, float radius, uint8_t layerMask) const
    {
        std::vector<BodyId> bodies;

        JPH::SphereShape sphere(radius);

        class Collector : public JPH::CollideShapeBodyCollector {
        public:
            std::vector<JPH::BodyID> bodyIds;

            void AddHit(const JPH::BodyID& bodyId) override {
                bodyIds.push_back(bodyId);
            }
        } collector;
        
        JPH::BroadPhaseLayerFilter broadPhaseLayerFilter;
        LayerMaskObjectFilter objectFilter(layerMask);

        m_Impl->PhysicsSystem->GetBroadPhaseQuery().CollideSphere(
            ToJolt(center),
            radius,
            collector,
            broadPhaseLayerFilter,
            objectFilter
        );

        for (const auto& id : collector.bodyIds) {
            bodies.push_back(id.GetIndexAndSequenceNumber());
        }

        return bodies;
    }

    std::vector<BodyId> PhysicsWorld::OverlapBox(const glm::vec3& center, const glm::vec3& halfExtent, const glm::quat& rotation, uint8_t layerMask) const
    {
        // TODO: What about rotation and layer mask??
        std::vector<BodyId> bodies;

        JPH::AABox box(
            ToJolt(center - halfExtent),
            ToJolt(center + halfExtent)
        );

        class Collector : public JPH::CollideShapeBodyCollector {
        public:
            std::vector<JPH::BodyID> bodyIds;

            void AddHit(const JPH::BodyID& bodyId) override {
                bodyIds.push_back(bodyId);
            }
        } collector;
        
        JPH::BroadPhaseLayerFilter broadPhaseLayerFilter;
        LayerMaskObjectFilter objectFilter(layerMask);

        m_Impl->PhysicsSystem->GetBroadPhaseQuery().CollideAABox(box, collector, broadPhaseLayerFilter, objectFilter);

        for (const auto& id : collector.bodyIds) {
            bodies.push_back(id.GetIndexAndSequenceNumber());
        }

        return bodies;
    }

    const std::vector<CollisionEvent>& PhysicsWorld::GetCollisionEnterEvents() const
    {
        return m_Impl->CollisionEnterEvents;
    }

    const std::vector<CollisionEvent>& PhysicsWorld::GetCollisionStayEvents() const
    {
        return m_Impl->CollisionStayEvents;
    }

    const std::vector<CollisionEvent>& PhysicsWorld::GetCollisionExitEvents() const
    {
        return m_Impl->CollisionExitEvents;
    }

    const std::vector<CharacterCollision>& PhysicsWorld::GetCharacterCollisions() const
    {
        return m_Impl->CharacterCollisions;
    }

    void PhysicsWorld::SetGravity(const glm::vec3& gravity)
    {
        m_Impl->Gravity = gravity;
        m_Impl->PhysicsSystem->SetGravity(ToJolt(gravity));
    }

    glm::vec3 PhysicsWorld::GetGravity() const
    {
        return m_Impl->Gravity;
    }
}
