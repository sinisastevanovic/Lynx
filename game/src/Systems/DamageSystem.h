#pragma once
#include "Lynx.h"
#include "Lynx/Scene/Components/UIComponents.h"
#include "Lynx/UI/Core/UIObjectPool.h"
#include "Lynx/UI/Widgets/UIText.h"

struct FloatingText
{
    std::shared_ptr<UIText> Element;
    glm::vec3 WorldPos;
    float LifeTime;
    float MaxLifeTime;
    float FloatSpeed = 50.0f;
    glm::vec3 Velocity = { 0.0f, 50.0f, 0.0f };
};

class DamageTextSystem
{
public:
    static void Init(std::shared_ptr<Scene> scene)
    {
        auto view = scene->View<UICanvasComponent>();
        for (auto e : view)
        {
            s_Canvas = view.get<UICanvasComponent>(e).Canvas;
            break;
        }

        if (!s_Canvas) 
            return;
        
        s_TextPool = std::make_unique<UIObjectPool<UIText>>([=]()
        {
            auto t = std::make_shared<UIText>();
            t->SetText("0");
            t->SetAnchor(UIAnchor::TopLeft);
            t->SetPivot({0.5f, 0.5f});
            t->SetTextAlignment(TextAlignment::Center);
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
    
    static void Update(float dt, std::shared_ptr<Scene> scene)
    {
        if (s_ActiveTexts.empty())
            return;
        
        glm::mat4 viewProj = Engine::Get().GetRenderer().GetCameraViewProjMatrix();
        
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
            glm::vec3 screenPos = MathHelpers::WorldToCanvas(item.WorldPos, viewProj, s_Canvas.get());
            
            if (screenPos.z < 0 || screenPos.z > 1)
            {
                item.Element->SetVisibility(UIVisibility::Hidden);
            }
            else
            {
                item.Element->SetVisibility(UIVisibility::Visible);
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
    static inline std::shared_ptr<UICanvas> s_Canvas;
    static inline std::unique_ptr<UIObjectPool<UIText>> s_TextPool;
    static inline std::vector<FloatingText> s_ActiveTexts;
};