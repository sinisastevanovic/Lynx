#pragma once
#include "Lynx/Engine.h"
#include "Lynx/Scene/Scene.h"
#include "Lynx/Scene/Components/UIComponents.h"
#include "Lynx/UI/Core/UIObjectPool.h"
#include "Lynx/UI/Widgets/UIText.h"
#include "Lynx/Utils/MathHelpers.h"


struct FloatingText
{
    std::shared_ptr<Lynx::UIText> Element;
    glm::vec3 WorldPos;
    float LifeTime;
    float MaxLifeTime;
    float FloatSpeed = 50.0f;
    glm::vec3 Velocity = { 0.0f, 50.0f, 0.0f };
};

class DamageTextSystem
{
public:
    static void Init(std::shared_ptr<Lynx::Scene> scene)
    {
        auto view = scene->Reg().view<Lynx::UICanvasComponent>();
        for (auto e : view)
        {
            s_Canvas = view.get<Lynx::UICanvasComponent>(e).Canvas;
            break;
        }

        if (!s_Canvas) 
            return;
        
        s_TextPool = std::make_unique<Lynx::UIObjectPool<Lynx::UIText>>([=]()
        {
            auto t = std::make_shared<Lynx::UIText>();
            t->SetText("0");
            t->SetAnchor(Lynx::UIAnchor::TopLeft);
            t->SetPivot({0.5f, 0.5f});
            t->SetTextAlignment(Lynx::TextAlignment::Center);
            t->SetFontSize(24);
            t->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
            s_Canvas->AddChild(t);
            return t;
        }, 50);
    }
    
    static void Spawn(glm::vec3 worldPos, int damage)
    {
        if (!s_TextPool || !s_Canvas)
            return;
        
        auto text = s_TextPool->Get();
        text->SetText(std::to_string(damage));
        text->SetOpacity(1.0f);
        
        text->SetFontSize(damage > 50 ? 32 : 24);
        s_ActiveTexts.push_back({text, worldPos, 1.0f, 1.0f});
    }
    
    static void Update(float dt, std::shared_ptr<Lynx::Scene> scene)
    {
        if (s_ActiveTexts.empty())
            return;
        
        glm::mat4 viewProj = Lynx::Engine::Get().GetRenderer().GetCameraViewProjMatrix();
        
        for (auto it = s_ActiveTexts.begin(); it != s_ActiveTexts.end(); )
        {
            auto& item = *it;
            item.LifeTime -= dt;
            
            if (item.LifeTime <= 0.0f)
            {
                s_TextPool->Return(item.Element);
                it = s_ActiveTexts.erase(it);
                continue;
            }
            
            item.WorldPos.y += item.FloatSpeed * dt * 0.01f;
            glm::vec3 screenPos = Lynx::MathHelpers::WorldToCanvas(item.WorldPos, viewProj, s_Canvas.get());
            
            if (screenPos.z < 0 || screenPos.z > 1)
            {
                item.Element->SetVisibility(Lynx::UIVisibility::Hidden);
            }
            else
            {
                item.Element->SetVisibility(Lynx::UIVisibility::Visible);
                item.Element->SetOffset({screenPos.x, screenPos.y});
            }
            
            float alpha = item.LifeTime / item.MaxLifeTime;
            item.Element->SetOpacity(alpha);
            
            ++it;
        }
    }
    
    static void Shutdown()
    {
        s_ActiveTexts.clear();
        s_TextPool.reset();
        s_Canvas.reset();
    }
    
private:
    static inline std::shared_ptr<Lynx::UICanvas> s_Canvas;
    static inline std::unique_ptr<Lynx::UIObjectPool<Lynx::UIText>> s_TextPool;
    static inline std::vector<FloatingText> s_ActiveTexts;
};