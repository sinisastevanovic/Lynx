#include "Lynx/Application.h"
#include "Lynx/EntryPoint.h"

class ExampleLayer : public Lynx::Layer
{
public:
    virtual void OnUIRender() override
    {
        ImGui::Begin("Hello");
        ImGui::Button("Button");
        ImGui::End();

        ImGui::ShowDemoWindow();
    }
};

Lynx::Application* Lynx::CreateApplication(int argc, char** argv)
{
    Lynx::ApplicationSpecification spec;
    spec.Name = "Lynx Project";

    Lynx::Application* app = new Lynx::Application(spec);
    app->PushLayer<ExampleLayer>();
    app->SetMenubarCallback([app]()
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit"))
            {
                app->Close();
            }

            ImGui::EndMenu();
        }
    });

    return app;
}
