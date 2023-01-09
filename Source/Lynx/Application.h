#pragma once

#include "Layer.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "imgui.h"
#include "vulkan/vulkan.h"

void check_vk_result(VkResult err);

struct GLFWwindow;

namespace Lynx
{
    struct ApplicationSpecification
    {
        std::string Name = "Lynx App";
        uint32_t Width = 1600;
        uint32_t Height = 900;
    };
    
    class Application
    {
    public:
        Application(const ApplicationSpecification& applicationSpecification = ApplicationSpecification());
        ~Application();

        static Application& Get();

        void Run();
        void Close();

        void SetMenubarCallback(const std::function<void()>& menubarCallback) { MenubarCallback_ = menubarCallback; }
        float GetTime();

        template<typename T>
        void PushLayer()
        {
            static_assert(std::is_base_of<Layer, T>::value, "Pushed type is not subclass of Layer!");
            LayerStack_.emplace_back(std::make_shared<T>())->OnAttach();
        }

        void PushLayer(const std::shared_ptr<Layer>& layer)
        {
            LayerStack_.emplace_back(layer);
            layer->OnAttach();
        }


        GLFWwindow* GetWindowHandle() const { return WindowHandle_; }

        static VkInstance GetInstance();
        static VkPhysicalDevice GetPhysicalDevice();
        static VkDevice GetDevice();

        static VkCommandBuffer GetCommandBuffer(bool begin);
        static void FlushCommandBuffer(VkCommandBuffer commandBuffer);

        static void SubmitResourceFree(std::function<void()>&& func);

    private:
        void Init();
        void Shutdown();

    private:
        ApplicationSpecification Specification_;
        GLFWwindow* WindowHandle_;
        bool Running_ = false;

        float TimeStep_ = 0.0f;
        float FrameTime_ = 0.0f;
        float LastFrameTime_ = 0.0f;

        std::vector<std::shared_ptr<Layer>> LayerStack_;
        std::function<void()> MenubarCallback_;
    
    };

    // Implemented by Client
    Application* CreateApplication(int argc, char** argv);
}

