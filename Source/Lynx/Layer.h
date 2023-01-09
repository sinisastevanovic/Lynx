#pragma once

namespace Lynx
{
    class Layer
    {
    public:
        virtual ~Layer() = default; // TODO: why default?

        virtual void OnAttach() {}
        virtual void OnDetach() {}

        virtual void OnUpdate(float deltaSeconds) {}
        virtual void OnUIRender() {}
    
    };
}

