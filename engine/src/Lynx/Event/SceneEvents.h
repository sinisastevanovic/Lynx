#pragma once

#include "Lynx/Core.h"
#include "Event.h"
#include "Lynx/Scene/Scene.h"

namespace Lynx
{
    class LX_API ActiveSceneChangedEvent : public Event
    {
    public:
        ActiveSceneChangedEvent(std::shared_ptr<Scene> scene)
            : m_Scene(scene) {}

        inline std::shared_ptr<Scene> GetScene() const { return m_Scene; }

        std::string ToString() const override
        {
            return "ActiveSceneChangedEvent";
        }

        EVENT_CLASS_TYPE(ActiveSceneChangedEvent)
    private:
        std::shared_ptr<Scene> m_Scene;
    };
}
