#pragma once

namespace Lynx
{
    class Component
    {
        
    };

    class TestComponent : Component
    {
    public:
        TestComponent() { LX_INFO("Ctor"); }
        TestComponent(int x, const std::string& name)
            : X(x), Name(name)
        {
            LX_INFO("Ctor with params: x: {} | name: {}", x, name);
        }

        int GetX() const { LX_INFO("GetX {}", X); return X; }
        void SetX(int newX) { X = newX; LX_INFO("SetX {}", newX);}
        std::string GetName() const { LX_INFO("GetName {}", Name); return Name; }
        void SetName(const std::string& newName) { Name = newName; LX_INFO("SetName {}", newName);}
        
    public:
        int X = 0;
        int32_t Y = 0;
        int64_t Int64 = 0;
        uint32_t Z = 0;
        std::string Name = "";
    };
}