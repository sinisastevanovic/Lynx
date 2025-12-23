#pragma once
#include "Lynx/Scene/ScriptableEntity.h"
#include "Lynx/Scene/Components/Components.h"

class TestNativeScript : public Lynx::ScriptableEntity
{
public:
    void OnCreate() override
    {
        LX_INFO("TestNativeScript::OnCreate");
    }

    void OnUpdate(float deltaTime) override
    {
        auto& transform = GetComponent<Lynx::TransformComponent>();
        transform.Translation.x += 5.0f * deltaTime;
    }

    void OnDestroy() override
    {
        LX_INFO("TestNativeScript::OnDestroy");
    }
};
